#!/usr/bin/awk -f

BEGIN {
    if ((getline cmd) <= 0) cmd = ""
    if ((getline old) <= 0) old = ""
    if ((getline shell) <= 0) shell = ""

    # Odtworzenie zachowania z Twojego main.c (zabezpieczenie pustej komendy)
    if (length(cmd) == 0) cmd = "l"

    if (length(old) > 0) {
        printf "export OLDPWD=%s;%s;set >&3", old, cmd
    } else {
        printf "%s;set >&3", cmd
    }
}
