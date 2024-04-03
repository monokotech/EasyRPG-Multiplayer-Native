#!/bin/bash

# requirements: lftp xxhash

source_url="$1"

dot_alias="${HIDDENFILE_DOT_ALIAS:-dotalias}_"

restore_dotfiles() {
	while read -r n; do
		rep="$(echo $(basename "$n") | sed "s/^$dot_alias/./")"
		mv "$n" "$(dirname "$n")/$rep"
	done < <(find -mindepth 1 -name "${dot_alias}*" | tac)
}

trap on_sigint SIGINT
on_sigint() {
	restore_dotfiles
	exit
}

# move dotfiles
while read -r n; do
	echo dotfile: $n
	rep=$(echo $(basename "$n") | sed "s/^\./$dot_alias/")
	mv "$n" "$(dirname "$n")/$rep"
done < <(find -mindepth 1 -name '.*' | tac)

lt="set net:timeout 5; set net:max-retries 3;"
lr="set net:reconnect-interval-multiplier 1; set net:reconnect-interval-base 3;"
ln="--no-symlinks --no-perms --no-umask"
lX="-X '*.lsd' -X Language -X save_dir -X easyrpg_log.txt"
lftp -e "$lt $lr mirror -c -e --parallel=8 $ln $lX -v . .; exit" "$source_url"

restore_dotfiles

echo Checking integrity...
IFS=
sums="$(xxh32sum -q -c xxh32)"
if [ $? -ne 0 ]; then
	echo $sums
	while read -r failed; do
		path="$(echo $failed | cut -d':' -f1)"
		# check path is within the current directory
		if [ "$(realpath "$path" | grep -o "^$(pwd)")" = "$(pwd)" ]; then
			rm -v "$path" | sed 's/^/(Corrupt) /'
		fi
	done < <(echo $sums | grep FAILED$)
else
	echo Passed
fi
