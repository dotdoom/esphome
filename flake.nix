{
  description = "esphome deployment shell";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      forAllSystems = f: nixpkgs.lib.genAttrs nixpkgs.lib.systems.flakeExposed (system: f {
        pkgs = import nixpkgs { inherit system; };
      });
    in
    {
      devShells = forAllSystems ({ pkgs }: {
        default = pkgs.mkShell {
          buildInputs = with pkgs; [
            esphome
            cc2538-bsl
          ];

          shellHook = ''
            echo -n "ESPHome "
            esphome --version
          '';
        };
      });
    };
}
