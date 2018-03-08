with (import <nixpkgs> {});
stdenv.mkDerivation {
  name = "delta-chat-core";
  buildInputs = [ meson ninja libetpan openssl sqlite zlib pkgconfig cyrus_sasl  bzip2 ];
  shellHook = ''
    export PS1="delta-chat> "
'';
}
