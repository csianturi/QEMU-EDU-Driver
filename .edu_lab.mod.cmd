savedcmd_/home/fatso/projects/edu_driver/edu_lab.mod := printf '%s\n'   edu_lab.o | awk '!x[$$0]++ { print("/home/fatso/projects/edu_driver/"$$0) }' > /home/fatso/projects/edu_driver/edu_lab.mod
