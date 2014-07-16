/********************************************************************************
Copyright(c) 2009 Analog Devices, Inc. All Rights Reserved.

This software is proprietary and confidential.  By using this software you agree
to the terms of the associated Analog Devices License Agreement.

USB Audio Device example application using BF52xc1 codec (Wolfson WM8731/L)
 
By: Wayne Dupont

*********************************************************************************/
#include <services/services.h>          /* system service includes      */
#include <drivers/adi_dev.h>            /* device manager includes      */

/* OSAL includes */
#include <osal/adi_osal.h>

#include "adi_ssl_init.h"
#include "codec.h"

/* Common Device Class Driver API includes */
#include <drivers/device_class/adi_dev_class.h>
/* Audio class driver includes */
#include <drivers/device_class/adi_dev_audio.h>

/* Define the record buffer size */
#define AUDIO_CODEC_ADC_BUFFER_SIZE             512

#define	USE_DATA_24_BIT							0 		/* Use 24 bit data for playback and record */

#define SSM_2603								0

#if SSM_2603
#define ADI_AUDIO_CLASS_DRIVER_ENTRY_POINT		&ADIDevAudioSSM2603EntryPoint
#else
#define ADI_AUDIO_CLASS_DRIVER_ENTRY_POINT		&ADIDevAudioBF52xC1EntryPoint
#endif

#define ADI_AUDIO_PHYSICAL_DEV_NUMBER			0

ADI_OSAL_SEM_HANDLE      hSemProcessAudioInData;
ADI_OSAL_SEM_HANDLE      hSemProcessAudioOutData;

#define	AUDIO_PLAYBACK_PROCESSING_THREAD_PRIO			14
#define	AUDIO_RECORD_PROCESSING_THREAD_PRIO				15

#define	PROCESSING_THREAD_STACK_SIZE					(1024 * 2)

static u32	gSubBufferSize;

static u32  gPlaybackState 	= STATE_PLAYBACK_PROCESS_NONE;
static u32  gRecordState 	= STATE_RECORD_PROCESS_NONE;

/* Memory area to use for the stack of the thread */
static char_t gaPlaybackProcessingStack[PROCESSING_THREAD_STACK_SIZE];
static char_t gaRecordProcessingStack[PROCESSING_THREAD_STACK_SIZE];

/* Global Audio Buffer for processing */
static ADI_DEV_1D_BUFFER *gAudioBuffer;

static ADI_DEV_AUDIO_DRIVER_BUFFER	outputBuffer;

/******* ENABLE THE DESCRETE FREQUENCY CODE *******/
#define DISCRETE_FREQ							0
/**************************************************/

/* Used to save current volume settings */
volatile u16				masterVolume	= 0;
volatile u16                masterMute		= 0;

volatile u16	            lineInVolume    = 0;
volatile u16	            lineInMute      = 0;

volatile u16	            recordSelect    = 0;
volatile u16	            recordGain    	= 0;

#define MAX_VOLUME			0x7FFF

static ADI_DEV_1D_BUFFER UsbAudioInBuffer;

static ADI_DEV_DEVICE_HANDLE AudioCodecDriverHandle;

extern ADI_DEV_DEVICE_HANDLE USBAudioClassHandle;

/*********************************************************************

	Function:		InitAudioCodec

*********************************************************************/
u32 adi_codec_Init(ADI_DCB_CALLBACK_FN pAudioCodecCallback,
				   u8 *pOutboundBuffer,
				   u32 bufferSize,
				   u32 subBufferSize)
{
	ADI_OSAL_THREAD_HANDLE hPlaybackProcessingThread;
    ADI_OSAL_THREAD_ATTR PlaybackProcessingThreadAttr;

	ADI_OSAL_THREAD_HANDLE hRecordProcessingThread;
    ADI_OSAL_THREAD_ATTR RecordProcessingThreadAttr;

    u32 nResult = ADI_DEV_RESULT_SUCCESS;

	do
	{
		/* Save output sub buffer size */
		gSubBufferSize = subBufferSize;

        /* Initialse Device Class Drivers */
        if ((nResult = adi_dev_Class_Init (adi_dev_ManagerHandle,
                                           adi_dma_ManagerHandle,
                                           NULL,
                                           NULL,
                                           0,
                                           false))
                                        != ADI_DEV_RESULT_SUCCESS)
        {
             printf("Failed to initialise Device Class Drivers, Error Code: 0x%08X\n", nResult);
            break;
        }

        /* Open Audio CODEC */
        if((nResult = adi_dev_Audio_Open (ADI_AUDIO_CLASS_DRIVER_ENTRY_POINT,
                                          ADI_AUDIO_PHYSICAL_DEV_NUMBER,
                                          pAudioCodecCallback,
                                          NULL,
                                          &AudioCodecDriverHandle))
                                       != ADI_DEV_RESULT_SUCCESS)
        {
            printf("Failed to open Audio device, Error Code: 0x%08X\n", nResult);
            break;
        }

		/* Setup our buffer information */
		outputBuffer.pBuffer = pOutboundBuffer;
		outputBuffer.nBufferSize = bufferSize;

        /* Set output buffer that can be used by the audio driver */
        if((nResult = adi_dev_Audio_Control(AudioCodecDriverHandle,
                                            ADI_DEV_AUDIO_CMD_SUBMIT_OUT_DATA_BUFFER,
                                            (void *)&outputBuffer))
                                         != ADI_DEV_RESULT_SUCCESS)
        {
            printf("Failed to submit the output buffer, Error Code: 0x%08X\n",nResult);
            break;
        }

		/* Set Audio callback buffer size to be the same as USB sub-buffer size so that
		   we could re-use the same buffer */
        if((nResult = adi_dev_Audio_Control(AudioCodecDriverHandle,
                                            ADI_DEV_AUDIO_CMD_SET_OUT_CALLBACK_BUFFER_SIZE,
                                            (void *)subBufferSize))
                                         != ADI_DEV_RESULT_SUCCESS)
        {
            printf("Failed to set audio output callback buffer size, Error Code: 0x%08X\n",nResult);
            break;
        }

        /* Set maximum empty buffer limit that can be requested */
        if((nResult = adi_dev_Audio_Control(AudioCodecDriverHandle,
                                            ADI_DEV_AUDIO_CMD_SET_MAX_EMPTY_BUFFER_REQUEST_SIZE,
                                            (void *)subBufferSize))
                                         != ADI_DEV_RESULT_SUCCESS)
        {
            printf("Failed to set maximum sub buffer size, Error Code: 0x%08X\n",nResult);
            break;
        }

#if USE_DATA_24_BIT
    	/* Set the output data length */
        if((nResult = adi_dev_Audio_SetWordLength (AudioCodecDriverHandle,
                                                   ADI_DEV_AUDIO_CHANNEL_ALL_OUT,
                                                   24))
                                                   != ADI_DEV_RESULT_SUCCESS)
        {
            printf("Failed to set audio out data word length, Error Code: 0x%08X\n", nResult);
            break;
        }
#else
    	/* Set the output data length */
        if((nResult = adi_dev_Audio_SetWordLength (AudioCodecDriverHandle,
                                                   ADI_DEV_AUDIO_CHANNEL_ALL_OUT,
                                                   16))
                                                   != ADI_DEV_RESULT_SUCCESS)
        {
            printf("Failed to set audio out data word length, Error Code: 0x%08X\n", nResult);
            break;
        }
#endif          	
        /* Submit Output channel sampling rate */
        if((nResult = adi_dev_Audio_SetSampleRate(AudioCodecDriverHandle,
                                                  ADI_DEV_AUDIO_CHANNEL_ALL_OUT,
                                                  48000,
                                                  false))
                                                  != ADI_DEV_RESULT_SUCCESS)
         {
             printf("Failed to set output channel sampling rate, Error Code: 0x%08X\n",nResult);
             break;
         }

         /* Submit Input channel sampling rate and apply cached sample rates */
         if((nResult = adi_dev_Audio_SetSampleRate(AudioCodecDriverHandle,
                                                   ADI_DEV_AUDIO_CHANNEL_ALL_IN,
                                                   48000,
                                                   true))
                                                   != ADI_DEV_RESULT_SUCCESS)
         {
             printf("Failed to set input channel sampling rate, Error Code: 0x%08X\n",nResult);
             break;
         }

    	/* Set Audio callback buffer size to be the same as USB sub-buffer size so that
		   we could re-use the same buffer */
        if((nResult = adi_dev_Audio_Control(AudioCodecDriverHandle,
                                            ADI_DEV_AUDIO_CMD_SET_IN_CALLBACK_BUFFER_SIZE,
                                            (void *)AUDIO_CODEC_ADC_BUFFER_SIZE))
                                         != ADI_DEV_RESULT_SUCCESS)
        {
            printf("Failed to set audio input callback buffer size, Error Code: 0x%08X\n",nResult);
            break;
        }

    	/* Set maximum empty buffer limit that can be requested by record */
    	if((nResult = adi_dev_Audio_Control(AudioCodecDriverHandle,
                                        ADI_DEV_AUDIO_CMD_SET_MAX_FULL_BUFFER_REQUEST_SIZE,
                                        (void *)AUDIO_CODEC_ADC_BUFFER_SIZE))
                                        != ADI_DEV_RESULT_SUCCESS)
    	{
        	printf("Failed to set maximum input buffer size to request, Error Code: 0x%08X\n",nResult);
    	}

#if USE_DATA_24_BIT

        /* Set the input data length */
        if((nResult = adi_dev_Audio_SetWordLength (AudioCodecDriverHandle,
                                                   ADI_DEV_AUDIO_CHANNEL_ALL_IN,
                                                   24))
                                                   != ADI_DEV_RESULT_SUCCESS)
        {
            printf("Failed to set audio in data word length, Error Code: 0x%08X\n", nResult);
            break;
        }

#else

    	/* Set the input data length */
        if((nResult = adi_dev_Audio_SetWordLength (AudioCodecDriverHandle,
                                                   ADI_DEV_AUDIO_CHANNEL_ALL_IN,
                                                   16))
                                                   != ADI_DEV_RESULT_SUCCESS)
        {
            printf("Failed to set audio in data word length, Error Code: 0x%08X\n", nResult);
            break;
        }

#endif

        /* Enable Left input channel */
        if((nResult = adi_dev_Audio_Control(AudioCodecDriverHandle,
                                            ADI_DEV_AUDIO_CMD_ENABLE_CHANNEL,
                                            (void*)ADI_DEV_AUDIO_CHANNEL_LINE_L_IN))
                                         != ADI_DEV_RESULT_SUCCESS)
        {
            printf("Failed to Enable Left input, Error Code: 0x%08X\n",nResult);
            break;
        }

        /* Enable Right input channel */
        if((nResult = adi_dev_Audio_Control(AudioCodecDriverHandle,
                                            ADI_DEV_AUDIO_CMD_ENABLE_CHANNEL,
                                            (void*)ADI_DEV_AUDIO_CHANNEL_LINE_R_IN))
                                         != ADI_DEV_RESULT_SUCCESS)
        {
            printf("Failed to Enable Right input, Error Code: 0x%08X\n",nResult);
            break;
        }

        /* Un-mute Input channels */
        if((nResult = adi_dev_Audio_Control(AudioCodecDriverHandle,
                                            ADI_DEV_AUDIO_CMD_UNMUTE_CHANNEL,
                                            (void*)ADI_DEV_AUDIO_CHANNEL_ALL_IN))
                                         != ADI_DEV_RESULT_SUCCESS)
        {
            printf("Failed to Un-mute input channels, Error Code: 0x%08X\n",nResult);
            break;
        }

        /* Set Input channel volume */
        if((nResult = adi_dev_Audio_SetVolume(AudioCodecDriverHandle,
                                              ADI_DEV_AUDIO_CHANNEL_ALL_IN,
                                              0xAfff))
                                           != ADI_DEV_RESULT_SUCCESS)
        {
            printf("Failed to set input channel volume, Error Code: 0x%08X\n",nResult);
            break;
        }

        /* Enable all Output channels */
        if((nResult = adi_dev_Audio_Control(AudioCodecDriverHandle,
                                            ADI_DEV_AUDIO_CMD_ENABLE_CHANNEL,
                                            (void*)ADI_DEV_AUDIO_CHANNEL_ALL_OUT))
                                         != ADI_DEV_RESULT_SUCCESS)
        {
            printf("Failed to Enable output channels, Error Code: 0x%08X\n",nResult);
            break;
        }

        /* Un-mute all Output channels */
        if((nResult = adi_dev_Audio_Control(AudioCodecDriverHandle,
                                            ADI_DEV_AUDIO_CMD_UNMUTE_CHANNEL,
                                            (void*)ADI_DEV_AUDIO_CHANNEL_ALL_OUT))
                                         != ADI_DEV_RESULT_SUCCESS)
        {
            printf("Failed to Un-mute output channels, Error Code: 0x%08X\n",nResult);
            break;
        }

         /* Create a semaphore to register audio out buffer processing event */
         if ((nResult = (u32) adi_osal_SemCreate(&hSemProcessAudioOutData,
                                                 0))
                                              != ADI_OSAL_SUCCESS)
         {
             printf("Failed to create semaphore for Processing Audio Out Data, Error Code: 0x%08X\n", nResult);
             break;
         }

         /* Create a semaphore to register audio in buffer processing event */
         if ((nResult = (u32) adi_osal_SemCreate(&hSemProcessAudioInData,
                                                 0))
                                              != ADI_OSAL_SUCCESS)
         {
             printf("Failed to create semaphore for Processing Audio In Data, Error Code: 0x%08X\n", nResult);
             break;
         }

         /* Create the Playback thread */
         PlaybackProcessingThreadAttr.pThreadFunc = Playback_Processing_Thread;
    	 PlaybackProcessingThreadAttr.nPriority = AUDIO_PLAYBACK_PROCESSING_THREAD_PRIO;
    	 PlaybackProcessingThreadAttr.pStackBase = gaPlaybackProcessingStack;
    	 PlaybackProcessingThreadAttr.nStackSize = sizeof(gaPlaybackProcessingStack);
    	 PlaybackProcessingThreadAttr.pParam = NULL;
    	 PlaybackProcessingThreadAttr.szThreadName = "Playback Audio Thread";

		 if((nResult = adi_osal_ThreadCreate(&hPlaybackProcessingThread,
		 									 &PlaybackProcessingThreadAttr))
		 									 != ADI_OSAL_SUCCESS)
		 {
            printf("Failed to create thread for Playback Processing thread, Error Code: 0x%08X\n", nResult);
            break;
		 }

         /* Create the Record thread */
         RecordProcessingThreadAttr.pThreadFunc = Record_Processing_Thread;
    	 RecordProcessingThreadAttr.nPriority = AUDIO_RECORD_PROCESSING_THREAD_PRIO;
    	 RecordProcessingThreadAttr.pStackBase = gaRecordProcessingStack;
    	 RecordProcessingThreadAttr.nStackSize = sizeof(gaRecordProcessingStack);
    	 RecordProcessingThreadAttr.pParam = NULL;
    	 RecordProcessingThreadAttr.szThreadName = "Record Audio Thread";

		 if((nResult = adi_osal_ThreadCreate(&hRecordProcessingThread,
		 									 &RecordProcessingThreadAttr))
		 									 != ADI_OSAL_SUCCESS)
		 {
            printf("Failed to create thread for Record Processing thread, Error Code: 0x%08X\n", nResult);
            break;
		 }

	}while(0);

	return(nResult);
}

void adi_codec_in_Start(void)
{
    /* Update record thread state to start audio recording */
    gRecordState = STATE_PROCESS_REC_DATA;
    /* Post semaphore to get record thread out from idle state */
    adi_osal_SemPost(hSemProcessAudioInData);
}

void adi_codec_in_Stop(void)
{
    /* Update record thread status */
	gRecordState = STATE_RECORD_PROCESS_NONE;
}

void adi_codec_in_Free_Buffer(void)
{
	adi_dev_AudioIn_SubmitEmptyBuffer (AudioCodecDriverHandle,
      							 	   512);
}

void adi_codec_out_Start(void)
{
   adi_osal_SemPost(hSemProcessAudioOutData);
}

void adi_codec_out_Stop(void)
{
    u32 nResult;

    /* Disable Audio out dataflow */
	if ((nResult = adi_dev_AudioOut_EnableDataflow(AudioCodecDriverHandle,
				                                   false))
        		                       		    != ADI_DEV_RESULT_SUCCESS)
	{
    	printf ("Failed to stop audio out dataflow, Error Code: 0x%08X\n",nResult);
	}
	gPlaybackState = STATE_PLAYBACK_PROCESS_NONE;
}

void adi_codec_SetMasterVolume( u16 codecData )
{
    u32 Result;

    /* Only update if there was a change */
    if(masterVolume != codecData)
    {
        /* Save the latest volume value */
    	masterVolume = codecData;

    	/* Set the DAC Volume */
    	if((Result = adi_dev_Audio_SetVolume(AudioCodecDriverHandle,
    						 ADI_DEV_AUDIO_CHANNEL_HP_OUT,
    						 (void *)masterVolume))
    						 != ADI_DEV_RESULT_SUCCESS)
    	{
    		printf("Failed to set DAC Volume, Error Code: 0x%8X\n",Result);
    	}
    }
}

void adi_codec_SetMasterMute( u16 codecData )
{
    u32 Result;

    /* Only update if there was a change */
    if(masterMute != codecData)
    {
	    /* Save the latest mute value */
	    masterMute = codecData & 1;

		if(masterMute == 1)
		{
    		/* DAC Mute */
			if((Result = adi_dev_Audio_Control(AudioCodecDriverHandle,
										 ADI_DEV_AUDIO_CMD_MUTE_CHANNEL,
										 (void *)ADI_DEV_AUDIO_CHANNEL_ALL_OUT))
							 			 != ADI_DEV_RESULT_SUCCESS)
			{
				printf("Failed to set DAC Mute, Error Code: 0x%8X\n",Result);
			}
		}
		else
		{
    		/* DAC UnMute */
			if((Result = adi_dev_Audio_Control(AudioCodecDriverHandle,
										 ADI_DEV_AUDIO_CMD_UNMUTE_CHANNEL,
										 (void *)ADI_DEV_AUDIO_CHANNEL_ALL_OUT))
							 			 != ADI_DEV_RESULT_SUCCESS)
			{
				printf("Failed to set DAC UnMute, Error Code: 0x%8X\n",Result);
			}
		}
    }
}

void adi_codec_SetLineInVolume( u16 codecData )
{
    u32 Result;

    /* Only update if there was a change */
    if(lineInVolume != codecData)
    {
        /* Save the latest volume value */
    	lineInVolume = codecData;

    	/* Set the Line In volume */
    	if((Result = adi_dev_Audio_SetVolume(AudioCodecDriverHandle,
    						 ADI_DEV_AUDIO_CHANNEL_LINE_IN,
    						 lineInVolume))
    						 != ADI_DEV_RESULT_SUCCESS)
    	{
    		printf("Failed to set Line In Volume, Error Code: 0x%8X\n",Result);
    	}
    }
}

void adi_codec_SetLineInMute( u16 codecData )
{
    u32 Result;

    /* Only update if there was a change */
    if(lineInMute != codecData)
    {
	    /* Save the latest mute value */
	    lineInMute = codecData & 1;

		if(lineInMute == 1)
		{
    		/* Mute */
			if((Result = adi_dev_Audio_Control(AudioCodecDriverHandle,
										 ADI_DEV_AUDIO_CMD_MUTE_CHANNEL,
										 (void *)ADI_DEV_AUDIO_CHANNEL_LINE_IN))
							 			 != ADI_DEV_RESULT_SUCCESS)
			{
				printf("Failed to set Line In Mute, Error Code: 0x%8X\n",Result);
			}
		}
		else
		{
    		/* UnMute */
			if((Result = adi_dev_Audio_Control(AudioCodecDriverHandle,
										 ADI_DEV_AUDIO_CMD_UNMUTE_CHANNEL,
										 (void *)ADI_DEV_AUDIO_CHANNEL_LINE_IN))
							 			 != ADI_DEV_RESULT_SUCCESS)
			{
				printf("Failed to set Line In UnMute, Error Code: 0x%8X\n",Result);
			}
		}
    }
}

void adi_codec_SetRecordMasterGain( u16 codecData )
{
    u32 Result;
	u16 tempGain;

    /* Only update if there was a change */
    if(recordGain != codecData)
    {
        /* Save the latest volume value */
    	recordGain = codecData;

    	/* Set the record Volume */
    	if((Result = adi_dev_Audio_SetVolume(AudioCodecDriverHandle,
    						 ADI_DEV_AUDIO_CHANNEL_LINE_IN,
    						 recordGain))
    						 != ADI_DEV_RESULT_SUCCESS)
    	{
    		printf("Failed to set Record Volume, Error Code: 0x%8X\n",Result);
    	}
    }
}

void adi_codec_GetMasterVolume( u16 *codecData )
{
    u32 Result;

    	/* Get the DAC Volume */
    	if((Result = adi_dev_Audio_GetVolume(AudioCodecDriverHandle,
    						 ADI_DEV_AUDIO_CHANNEL_HP_OUT,
    						 codecData))
    						 != ADI_DEV_RESULT_SUCCESS)
    	{
    		printf("Failed to get DAC Volume, Error Code: 0x%8X\n",Result);
    	}
}

void adi_codec_GetLineInVolume( u16 *codecData )
{
    u32 Result;

    	/* Get the Line Volume */
    	if((Result = adi_dev_Audio_GetVolume(AudioCodecDriverHandle,
    						 ADI_DEV_AUDIO_CHANNEL_LINE_IN,
    						 codecData))
    						 != ADI_DEV_RESULT_SUCCESS)
    	{
    		printf("Failed to get Line Volume, Error Code: 0x%8X\n",Result);
    	}
}

#if DISCRETE_FREQ
u32 adi_codec_ProgramSampleRate(u32 sampleRate)
{
    u32 Result = ADI_DEV_RESULT_SUCCESS;

#if 0
    printf("Sample Rate = %dHz\n", sampleRate);
#endif

    /* Submit Output channel sampling rate */
    Result = adi_dev_Audio_SetSampleRate(AudioCodecDriverHandle,
                                ADI_DEV_AUDIO_CHANNEL_ALL_OUT,
                                sampleRate,
                                false);

	/* Submit Input channel sampling rate and apply cached sample rates */
    Result = adi_dev_Audio_SetSampleRate(AudioCodecDriverHandle,
                                ADI_DEV_AUDIO_CHANNEL_ALL_IN,
                                sampleRate,
                                true);

	return Result;
}
#endif

/**************************************************************************************
*
*	Playback
*
***************************************************************************************/
void ProcessAudioOutData(ADI_DEV_1D_BUFFER *pBuffer)
{
	/* Set the audio puffer pointer */
	gAudioBuffer = pBuffer;

	/* Post the Audio Process Data semaphore */
    adi_osal_SemPost(hSemProcessAudioOutData);
}

/**************************************************************************************
*
*	Record
*
***************************************************************************************/
void ProcessAudioInData()
{
	/* Post the Audio In Process Data semaphore */
    adi_osal_SemPost(hSemProcessAudioInData);
}

/**************************************************************************************
***************************************************************************************
***************************************************************************************
********************* These are the main audio processing threads *********************
***************************************************************************************
***************************************************************************************
***************************************************************************************/

/**************************************************************************************
*
*	Thread to Process the Playback events
*
**************************************************************************************/
static void Playback_Processing_Thread(void)
{
    u32 nResult = ADI_DEV_RESULT_SUCCESS;
	u32 nRunning = true;
	u32 nBytesRead;

    /* Pointer to empty output buffer */
    void        *pEmptyOutBuffer;

    while(nRunning)
    {
    	switch(gPlaybackState)
    	{
    		case STATE_START_PBK_DATA_FLOW:
    		{
	 			/* Enable Audio out codec dataflow */
  	 			if ((nResult = adi_dev_AudioOut_EnableDataflow(AudioCodecDriverHandle,
           					                                  true))
           	        		                        		  != ADI_DEV_RESULT_SUCCESS)
	 			{
       				printf ("Failed to enable audio out dataflow, Error Code: 0x%08X\n",nResult);
     			}

     			gPlaybackState = STATE_PROCESS_PBK_DATA;
    		}
    		break;

    		case STATE_PROCESS_PBK_DATA:

    		{
        		/* Pend until we have playback started */
        		adi_osal_SemPend (hSemProcessAudioOutData, ADI_OSAL_TIMEOUT_FOREVER);

        		/* Query for an empty audio buffer */
        		nResult = adi_dev_AudioOut_GetEmptyBuffer (AudioCodecDriverHandle,
                		                                   (void *) &pEmptyOutBuffer,
                        		                           gSubBufferSize);

				/* Now we can copy the data buffers and submit then to the codec */
				if(nResult == ADI_DEV_RESULT_SUCCESS)
        		{
 	        		/* Release the output buffer */
    	    		nResult = adi_dev_AudioOut_SubmitFullBuffer (AudioCodecDriverHandle, gSubBufferSize);

					if(nResult != ADI_DEV_RESULT_SUCCESS)
    	    		{
    	    			printf("adi_dev_AudioOut_SubmitFullBuffer Failed : %x\n", nResult);
    	    		}
        		}
        		else
        		{
        			printf("adi_dev_AudioOut_GetEmptyBuffer Failed : %x\n", nResult);
        		}
	 		}
    		break;

    		case STATE_PLAYBACK_PROCESS_NONE:
    		{
        		adi_osal_SemPend (hSemProcessAudioOutData, ADI_OSAL_TIMEOUT_FOREVER);

        		gPlaybackState = STATE_START_PBK_DATA_FLOW;
    		}
    		break;

    		default:
    		break;
    	}
    }
}

/**************************************************************************************
*
*	Thread to Process the Record events
*
**************************************************************************************/
static void Record_Processing_Thread(void)
{
    bool bDataflow = false;
    u32 nResult = ADI_DEV_RESULT_SUCCESS;
	u32 nRunning = true;
	u32 nBytesRead;
	u32 nRecordSemCount;
    ADI_OSAL_STATUS eStatus;

    /* Pointer to full input buffer */
    void *pFullInBuffer;

    while(nRunning)
    {
    	switch(gRecordState)
    	{
    		case STATE_START_REC_DATA_FLOW:
    		break;

    		case STATE_PROCESS_REC_DATA:
    		{
    		    /* IF (Record dataflow is disabled) */
    		    if (bDataflow == false)
    		    {
    		        /* Enable Audio in codec dataflow */
  	 			    if ((nResult = adi_dev_AudioIn_EnableDataflow(AudioCodecDriverHandle,
           					                                      true))
           	        		                        		   != ADI_DEV_RESULT_SUCCESS)
    	 			{
           				printf ("Failed to enable audio in dataflow, Error Code: 0x%08X\n",nResult);
     	    		}
     	    		bDataflow = true;
    		    }

        		adi_osal_SemPend (hSemProcessAudioInData, ADI_OSAL_TIMEOUT_FOREVER);

        		nBytesRead = AUDIO_CODEC_ADC_BUFFER_SIZE;

        		/* Query for a full audio buffer */
        		nResult = adi_dev_AudioIn_GetFullBuffer (AudioCodecDriverHandle,
                		                                 (void *)&pFullInBuffer,
                		                                 nBytesRead);

				if(nResult != ADI_DEV_RESULT_SUCCESS)
   	    		{
   	    		    /* failed to get a full input buffer. don't submit buffer to usb */
   	    			break;
   	    		}

		   		UsbAudioInBuffer.Data                = (void *)pFullInBuffer;
				UsbAudioInBuffer.ElementCount        = nBytesRead;
		   		UsbAudioInBuffer.ElementWidth        = 1;
		  		UsbAudioInBuffer.CallbackParameter   = &UsbAudioInBuffer;
				UsbAudioInBuffer.ProcessedFlag       = FALSE;
		  		UsbAudioInBuffer.pNext           	 = NULL;

               	adi_dev_Write(USBAudioClassHandle,
                   	          ADI_DEV_1D,
                       	      (ADI_DEV_BUFFER *)&UsbAudioInBuffer);

                nResult = adi_dev_AudioIn_SubmitEmptyBuffer (AudioCodecDriverHandle,
                											 nBytesRead);
       		}
    		break;

    		case STATE_RECORD_PROCESS_NONE:
            {
                /* IF (Record dataflow is enabled) */
    		    if (bDataflow == true)
    		    {
                    /* Disable Audio input dataflow */
	                adi_dev_AudioIn_EnableDataflow(AudioCodecDriverHandle, false);
	                bDataflow = false;
    		    }

	            /* Get present count of Audio input process semaphore */
	            eStatus = adi_osal_SemQuery (hSemProcessAudioInData, &nRecordSemCount);

            	if (eStatus != ADI_OSAL_SUCCESS)
            	{
	                printf ("Failed to query input audio process semaphore count\n");
	            }
	            else
            	{
            	    /* Clear record semaphore count */
    	            while (nRecordSemCount != 0)
	                {
	                    adi_osal_SemPend(hSemProcessAudioInData, 0);
    	                nRecordSemCount--;
	                }
	            }

	            /* enter idle state until record is started again */
        		adi_osal_SemPend (hSemProcessAudioInData, ADI_OSAL_TIMEOUT_FOREVER);
            }
    		break;

    		default:
    		break;
    	}
    }
}