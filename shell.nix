with import <nixpkgs> {};

llvmPackages_12.stdenv.mkDerivation {
  name = "hat-rs";
  nativeBuildInputs = [ cmake pkg-config ];

  buildInputs = [
    SDL2 sfml stb fmt glm
  ];
}