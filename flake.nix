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
          pre-commit-check = fw_nix.inputs.git-hooks.lib.${system}.run (
            fw_nix.lib.pre-commit
            // {
              src = ./.;
              hooks = fw_nix.lib.pre-commit.hooks // {
                # Disable check-yaml because it fails on ESPHome's custom YAML tags (like !include, !secret)
                check-yaml.enable = false;
              };
            }
          );
        }
      );

      devShells = forAllSystems (
        { system, pkgs, ... }:
        let
          esphome-direct = pkgs.esphome.override { platformio = pkgs.platformio-core; };
          esphome-fhs = pkgs.buildFHSEnv {
            name = "esphome-fhs";
            targetPkgs =
              pkgs: with pkgs; [
                esphome-direct
                git
                glibc
                zlib
                libusb1
                gcc
                gnumake
              ];
            runScript = "esphome";
          };
          esphome-pkg =
            if pkgs.stdenv.hostPlatform.isLinux then
              pkgs.writeShellScriptBin "esphome" ''
                exec ${esphome-fhs}/bin/esphome-fhs "$@"
              ''
            else
              esphome-direct;
        in
        {
          default = pkgs.mkShell {
            buildInputs = with pkgs; [
              esphome-pkg
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

          # CI shell: uses the same esphome version as default, but overrides
          # platformio to use platformio-core instead of the FHS-wrapped version.
          # This avoids nixpkgs wrapping platformio in bwrap, which fails
          # on CI runners that disallow unprivileged user namespaces.
          ci = pkgs.mkShell {
            buildInputs = with pkgs; [
              esphome-direct
              esptool
              gnumake
            ];

            shellHook = ''
              echo -n "ESPHome (CI) "
              esphome --version
            '';
          };
        }
      );
    };
}
