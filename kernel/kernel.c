/* This will force us to create a kernel entry function instead of jumping to kernel.c:0x00 */
void dummy_test_entrypoint() {
}

void main() {
    char hello_world[30] = "CasseOS kernel started!";
    char* video_memory = (char*) 0xb8000;
    for(int i=0;i<20;i++){
        *video_memory = hello_world[i];
        video_memory+=2;
    }
    for(int i=0;i<25*80-20;i++){
        *video_memory = 0;
        video_memory+=2;
    }
}
