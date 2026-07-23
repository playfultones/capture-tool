{
  description = "Reference Capture Tool - JUCE 8 audio capture app (dev shell)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      # Build tooling only. The C/C++ compiler is intentionally Apple clang
      # from /usr/bin (a nix clang shadowing `cc` breaks macOS/JUCE builds),
      # so this shell deliberately provides NO compiler.
      systems = [ "aarch64-darwin" "x86_64-darwin" ];
      forAllSystems = f:
        nixpkgs.lib.genAttrs systems (system: f (import nixpkgs { inherit system; }));
    in
    {
      devShells = forAllSystems (pkgs: {
        default = pkgs.mkShell {
          packages = [
            pkgs.cmake
            pkgs.ninja
            pkgs.pkg-config
            # e2e automation driver (juce-end-to-end TS library)
            pkgs.nodejs_22
          ];

          shellHook = ''
            echo "capture-tool devshell: cmake $(cmake --version | head -1 | awk '{print $3}'), ninja $(ninja --version)"
            echo "Compiler: Apple clang from /usr/bin (nix provides no compiler here)."
          '';
        };
      });
    };
}
