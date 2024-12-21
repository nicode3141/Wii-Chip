#include <stdio.h>
#include <stdlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <asndlib.h>
#include <mp3player.h>
#include <ogc/lwp.h>
#include <unistd.h>

#include "cJSON.h"

// include generated header
#include "sample_mp3.h"
#include "Dataset_json.h"

static void *xfb = NULL;
static GXRModeObj *rmode = NULL;

#define STACK_SIZE 8192
static lwp_t ascii_thread;
static lwp_t mp3_thread;
static u8 thread_stack[STACK_SIZE];
static u8 thread_stack2[STACK_SIZE];

// Shared data
static volatile int thread_running = 1;

typedef struct {
    cJSON *frames;
    int frame_count;
} StreamAsciiArgs;

void streamAsciiThread(cJSON *frames, int frame_count) {
    
	while(1){
		for (int i = 0; i < frame_count; i++) {
			cJSON *frame = cJSON_GetArrayItem(frames, i);

			cJSON *frame_item = NULL;
			cJSON_ArrayForEach(frame_item, frame) {
				const char *value = cJSON_GetStringValue(frame_item);
				printf("%s\n", value);
				usleep(70000);
				printf("\x1b[2J");
				printf("\x1b[0;0H");
			}
		}

		WPAD_ScanPads();
		u32 pressed = WPAD_ButtonsDown(0);

		if ( pressed & WPAD_BUTTON_HOME ){
			thread_running = 0; 
			exit(0);
		};

		VIDEO_WaitVSync();
	}

    return NULL;
}

void *mp3PlayerThread(void *arg){
	while(1){
		if(!MP3Player_IsPlaying()){MP3Player_PlayBuffer(sample_mp3, sample_mp3_size, NULL);}
		usleep(1000000);
	}
}

int main(int argc, char **argv) {
	VIDEO_Init();

	WPAD_Init();

	ASND_Init();
	MP3Player_Init();

	rmode = &TVEurgb60Hz480IntDf;

	xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(rmode));

	console_init(xfb,120,90,rmode->fbWidth,rmode->xfbHeight,rmode->fbWidth*VI_DISPLAY_PIX_SZ);

	VIDEO_Configure(rmode);

	VIDEO_SetNextFramebuffer(xfb);

	VIDEO_SetBlack(FALSE);

	VIDEO_Flush();
	VIDEO_WaitVSync();
	if(rmode->viTVMode&VI_NON_INTERLACE) VIDEO_WaitVSync();

    cJSON *json = cJSON_Parse((const char *)Dataset_json);
    if(json == NULL){
        const char *error_ptr = cJSON_GetErrorPtr();
        if(error_ptr != NULL){
            printf("Error: %s\n", error_ptr);
        }

        cJSON_Delete(json);
        return 1;
    }

    cJSON *frames = cJSON_GetObjectItemCaseSensitive(json, "frames");

    if (!cJSON_IsArray(frames)) {
        printf("Kein 'frames' Array in der JSON-Datei gefunden.\n");
        cJSON_Delete(json);
        return 1;
    }

    int frame_count = cJSON_GetArraySize(frames);

	printf("\x1b[0;0H");
	
	StreamAsciiArgs thread_args = {
    .frames = frames,
    .frame_count = frame_count
	};

	//create thread to play mp3
	LWP_CreateThread(&mp3_thread, mp3PlayerThread, NULL, thread_stack2, STACK_SIZE, 50);
	LWP_SetThreadPriority(mp3_thread, LWP_PRIO_HIGHEST);

	streamAsciiThread(frames, frame_count);
	return 0;
}
