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

    createPromise(name, timeoutMs) {
        const promiseId = this.lastPromiseId++;
        const result = new Promise((resolve, reject) => {
            // A native function name that C++ never registered produces no
            // __juce__complete event, so without this the promise never settles
            // and every `await backend.call(...)` after it is dead code. That is
            // exactly how a stale call to the unregistered 'getRecordingTailMs'
            // silently truncated refreshAllUIState() and left project loads with
            // a half-populated UI. Fail loudly instead.
            const timer = timeoutMs > 0
                ? setTimeout(() => {
                    if (this.promises.delete(promiseId)) {
                        reject(new Error(
                            `backend.call('${name}') did not respond within ${timeoutMs} ms — ` +
                            `is it registered as a native function in MainComponent?`));
                    }
                }, timeoutMs)
                : null;

            const done = () => { if (timer !== null) clearTimeout(timer); };
            this.promises.set(promiseId, {
                resolve: (value) => { done(); resolve(value); },
                reject: (error) => { done(); reject(error); },
            });
        });
        return [promiseId, result];
    }
}

const promiseHandler = new PromiseHandler();

/** Default response deadline for a native call, in ms. */
const DEFAULT_CALL_TIMEOUT_MS = 20000;

/**
 * Per-call deadline overrides. 0 disables the deadline entirely.
 *
 * The browse* functions open a MODAL native file chooser and do not complete
 * until the user dismisses it, which is unbounded by design — they must never be
 * timed out. Everything else is expected to answer promptly; if it does not,
 * that is a bug worth surfacing rather than a hang worth tolerating.
 */
const CALL_TIMEOUT_OVERRIDES_MS = {
    browseOutputFolder: 0,
    browseAndAddReferenceSignals: 0,
};

/**
 * Get a callable function for a registered native function
 * @param {string} name - Function name registered on backend
 * @returns {function} Async function that calls the backend
 */
function getNativeFunction(name) {
    const timeoutMs = name in CALL_TIMEOUT_OVERRIDES_MS
        ? CALL_TIMEOUT_OVERRIDES_MS[name]
        : DEFAULT_CALL_TIMEOUT_MS;

    return function(...args) {
        const [promiseId, result] = promiseHandler.createPromise(name, timeoutMs);

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
