import subprocess
import os
import sys

bootloader_asm=""
with open("bootloader/bios/bootloader.asm") as f:
    bootloader_asm = f.read()
sections = bootloader_asm.split("NUM_SECTORS")
section = sections[3].split("%endif")
num_sectors_available = eval(section[0])

env = os.environ.copy()
env["KERNEL_BIN_PATH"] = sys.argv[1]
result = subprocess.run(["./scripts/num_sectors.sh","--no-verbose"], capture_output=True, text=True,env=env)
output = result.stdout
if result.stderr!='':
    print(result.stderr)
    sys.exit(1)

num_sectors_necessary=eval(output)

if(num_sectors_available<num_sectors_necessary):
    print("You need",num_sectors_necessary,"sectors to fit in the kernel, but you did only set",num_sectors_available,"in the bootloader!")
    sys.exit(1)


sys.exit(0)
