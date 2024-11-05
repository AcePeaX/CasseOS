# $@ = target file
# $< = first dependency
# $^ = all dependencies



all:
	make bin/kernel.bin
	make bin/bootloader.bin
	./scripts/concat_and_launch.sh

virtualbox:
	make bin/kernel.bin
	make bin/bootloader.bin
	./scripts/concat_and_launch.sh --virtualbox

# Notice how dependencies are built as needed
bin/kernel.bin: bootloader/*
	./scripts/compile_kernel.sh


bin/bootloader.bin: kernel/*
	./scripts/compile_bootloader.sh

bin/os-image.bin: bin/kernel.bin bin/bootloader.bin
	./scripts/concat_and_launch.sh

kernel:	bin/kernel.bin

bootloader: bin/bootloader.bin

clean:
	rm bin/*.bin bin/*.o bin/*.img bin/*.vdi
