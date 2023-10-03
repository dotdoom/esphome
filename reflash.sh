#!/usr/bin/env zsh
# Build a full list of known esphome devices and
#   - reflash them
#   - reflash those that match $1
#   - do $1 to those that match $2
#
# Ex.: ./reflash.sh logs car

set -e

# Make $variable expand into multiple argv when unquoted.
set -o shwordsplit

if [ $# -gt 1 ]; then
	command=$1
	shift
else
	command="run --no-logs"
fi

flash=(
	"${command?} bathroomheater.yaml"
	"${command?} blinds.yaml"
	"${command?} boiler.yaml"
	"${command?} ruuvi.yaml"
	"${command?} watermeter.yaml"
)

for btproxy_replica in \
	c25bac
do
	flash+=("${command?} btproxy.yaml --device btproxy-${btproxy_replica?}")
done

for mystrom_appliance in \
	car-charger \
	freezer \
	living-room-entertainment \
	outdoor-deer \
	pool-pump \
	recirculation-pump \
	storage-heater \
	tools-power
do
	custom_file="${mystrom_appliance?}.yaml"
	if [ -f "${custom_file?}" ]; then
		flash+=("${command?} ${custom_file?}")
	else
		flash+=("-s name ${mystrom_appliance?} ${command?} templates/mystrom.yaml")
	fi
done

declare -A flash_result

print_results_table() {
	printf -- "-%.0s" {1..80}
	printf "\n%-70sSTATUS\n\n" "COMMAND"
	for result_cmdline in "${flash[@]}"; do
		printf "%-70s" "$result_cmdline "
		if [ "${flash_result["$result_cmdline"]+placeholder}" ]; then
			echo "${flash_result["$result_cmdline"]}"
		else
			echo "N/A"
		fi
	done
	printf "\n%-70s%d\n" "TOTAL" "${#flash[@]}"
	printf -- "-%.0s" {1..80}
	echo
	echo
}

for cmdline in "${flash[@]}"; do
	if [ $# -gt 0 ]; then
		if [[ "$cmdline" =~ "$1" ]]; then
			:;
		else
			flash_result["$cmdline"]=SKIPPED
			continue
		fi
	fi
	flash_result["$cmdline"]=CURRENT

	print_results_table
	echo "Launching esphome: $cmdline"
	if esphome $cmdline; then
		flash_result["$cmdline"]=OK
	else
		flash_result["$cmdline"]=FAIL
	fi
done

print_results_table
