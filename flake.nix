{
  description = "esphome deployment shell";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    fw_nix = {
      url = "github:futureware-tech/nix";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      fw_nix,
    }:
    let
      forAllSystems =
        f:
        nixpkgs.lib.genAttrs nixpkgs.lib.systems.flakeExposed (
          system:
          f {
            inherit system;
            pkgs = import nixpkgs { inherit system; };
          }
        );
    in
    {
      checks = forAllSystems (
        { system, ... }: {
          pre-commit-check = fw_nix.inputs.git-hooks.lib.${system}.run {
            src = ./.;
            hooks = fw_nix.lib.pre-commit.hooks // {
              # Disable check-yaml because it fails on ESPHome's custom YAML tags (like !include, !secret)
              check-yaml.enable = false;
            };
          };
        }
      );

      devShells = forAllSystems (
        { system, pkgs, ... }:
        {
          default = pkgs.mkShell {
            buildInputs = with pkgs; [
              esphome
              esptool
              cc2538-bsl
              gnumake
            ];

            shellHook = ''
              ${self.checks.${system}.pre-commit-check.shellHook}
              echo -n "ESPHome "
              esphome --version
            '';
          };
        }
      );
    };
}
