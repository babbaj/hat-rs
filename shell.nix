with import <nixpkgs> {};

llvmPackages_12.stdenv.mkDerivation {
  name = "hat-rs";
  nativeBuildInputs = [ cmake ];

  buildInputs = [
    SDL2 sfml stb
  ];
}