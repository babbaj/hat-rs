with import <nixpkgs> {};

llvmPackages_11.stdenv.mkDerivation {
  name = "hat-rs";
  nativeBuildInputs = [ cmake llvmPackages_11.libcxxClang ];

  buildInputs = [
    SDL2 sfml
  ];
}