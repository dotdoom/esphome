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

          # CI shell: uses the same esphome version as default, but installs it
          # via pipx to avoid nixpkgs wrapping platformio in bwrap, which fails
          # on CI runners that disallow unprivileged user namespaces.
          ci = pkgs.mkShell {
            buildInputs = with pkgs; [
              python3
              python3Packages.pipx
              esptool
              gnumake
            ];

            shellHook = ''
              pipx install --quiet esphome==${pkgs.esphome.version}
              export PATH="$HOME/.local/bin:$PATH"
              echo -n "ESPHome (CI) "
              esphome --version
            '';
          };
        }
      );
    };
}
