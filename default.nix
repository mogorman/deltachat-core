with (import <nixpkgs> {});
stdenv.mkDerivation {
  name = "delta-chat-core";
  buildInputs = [ libetpan meson ninja openssl sqlite zlib pkgconfig cyrus_sasl  bzip2 ];
  shellHook = ''
    export PS1="delta-chat> "
'';
}
