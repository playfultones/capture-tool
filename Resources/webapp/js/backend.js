// Backend communication module
// Handles promise-based calls to native C++ functions

/**
 * Promise handler for native function calls
 */
class PromiseHandler {
    constructor() {
        this.lastPromiseId = 0;
        this.promises = new Map();

        // Listen for function completion events from backend
        if (window.__JUCE__?.backend) {
            window.__JUCE__.backend.addEventListener(
                "__juce__complete",
                ({ promiseId, result }) => {
                    if (this.promises.has(promiseId)) {
                        this.promises.get(promiseId).resolve(result);
                        this.promises.delete(promiseId);
                    }
                }
            );
        }
    }

    createPromise() {
        const promiseId = this.lastPromiseId++;
        const result = new Promise((resolve, reject) => {
            this.promises.set(promiseId, { resolve, reject });
        });
        return [promiseId, result];
    }
}

const promiseHandler = new PromiseHandler();

/**
 * Get a callable function for a registered native function
 * @param {string} name - Function name registered on backend
 * @returns {function} Async function that calls the backend
 */
function getNativeFunction(name) {
    return function(...args) {
        const [promiseId, result] = promiseHandler.createPromise();

        window.__JUCE__.backend.emitEvent("__juce__invoke", {
            name: name,
            params: args,
            resultId: promiseId,
        });

        return result;
    };
}

/**
 * Backend API wrapper
 * Provides a clean interface for calling C++ backend functions
 */
const backend = {
    /**
     * Call a native C++ function by name
     * @param {string} name - Function name registered on backend
     * @param {...any} args - Arguments to pass to the function
     * @returns {Promise<any>} Result from the backend
     */
    async call(name, ...args) {
        if (!window.__JUCE__?.backend) {
            throw new Error('Backend not available');
        }
        
        const fn = getNativeFunction(name);
        return fn(...args);
    },

    /**
     * Register a handler for backend events
     * @param {string} eventId - Event identifier
     * @param {function} handler - Callback function
     * @returns {function} Unsubscribe function
     */
    onEvent(eventId, handler) {
        if (!window.__JUCE__?.backend) {
            console.warn('Backend not available, event listener not registered');
            return () => {};
        }
        
        const token = window.__JUCE__.backend.addEventListener(eventId, handler);
        return () => window.__JUCE__.backend.removeEventListener(token);
    },
};
