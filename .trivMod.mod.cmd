savedcmd_/home/student/kernel_module/trivMod.mod := printf '%s\n'   trivMod.o | awk '!x[$$0]++ { print("/home/student/kernel_module/"$$0) }' > /home/student/kernel_module/trivMod.mod
