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

declare -a flash

for config in \
		bathroomheater.yaml \
		blinds.yaml \
		heating.yaml \
		loud-siren.yaml \
		ruuvi.yaml \
		watermeter.yaml \
		heating-eg.yaml \
		heating-og.yaml \
		car-charger.yaml \
		freezer.yaml \
		living-room-entertainment.yaml \
		outdoor-equipment.yaml \
		recirculation-pump.yaml \
		storage-heater.yaml \
		basement-rack.yaml \
		tools-power.yaml; do
	flash+=("${command?} ${config?}")
done

for btproxy_replica in \
	c25bac
do
	flash+=("${command?} btproxy.yaml --device btproxy-${btproxy_replica?}")
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
