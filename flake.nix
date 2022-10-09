{
  description = "A very basic flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
    vm-tools.url = "github:/babbaj/vm-tools";
    vm-tools.inputs.nixpkgs.follows = "nixpkgs";
  };

  outputs = { self, nixpkgs, vm-tools }:
  let
    system = "x86_64-linux";
    pkgs = import nixpkgs { inherit system; };
  in {
    devShell.${system} = pkgs.mkShell {
      buildInputs = with pkgs; [
        SDL2 sfml stb fmt glm
        vm-tools.packages.${system}.snuggleheimer
      ];
      nativeBuildInputs = with pkgs; [
        pkg-config
        cmake
        clang_14
        vm-tools.packages.${system}.dump-il2cpp
      ];

      shellHook = ''
        export CC=clang
        export CXX=clang++
      '';
    };
  };
}
