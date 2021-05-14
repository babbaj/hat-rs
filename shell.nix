with import <nixpkgs> {};

llvmPackages_11.stdenv.mkDerivation {
  name = "hat-rs";
  nativeBuildInputs = [ cmake ];

  buildInputs = [
    SDL2 sfml
  ];
}