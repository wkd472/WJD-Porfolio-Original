/********************************************************************************
Copyright(c) 2008 Analog Devices, Inc. All Rights Reserved.

This software is proprietary and confidential.  By using this software you agree
to the terms of the associated Analog Devices License Agreement.

USB Audio Device example application 

By: Wayne Dupont 

*********************************************************************************/
#include <services\services.h>		        /* System service includes */
#include <drivers\adi_dev.h>		        /* Device manager includes */

#include <drivers/sport/adi_sport.h>
#include <drivers/codec/adi_ac97.h>
#include <drivers/codec/adi_ad1980.h>

#include <drivers\usb\usb_core\adi_usb_objects.h>
#include <drivers\usb\usb_core\adi_usb_core.h>
#include <drivers\usb\usb_core\adi_usb_ids.h>

#include <drivers\usb\usb_core\adi_usb_logevent.h>

#include <drivers\usb\controller\otg\adi\bf54x\adi_usb_bf54x.h>

#include <drivers\usb\class\peripheral\audio\adi_usb_audio_class.h>

#include "adi_ssl_init.h"

#include <stdio.h>

#include "codec.h"


#define 	PBK_PKT_SIZE							192 * NUM_AUDIO_DAC_CHANNELS

#define     AUDIO_CODEC_DAC_BUFFER_SIZE           	PBK_PKT_SIZE * 16

/* 
    1D buffers to manage Outbound Audio data
    these buffers are passed to the codec driver
*/
static ADI_DEV_1D_BUFFER OutboundBuffer[NUM_DAC_BUFFERS];
static ADI_DEV_1D_BUFFER UsbOutboundBuffer[NUM_DAC_BUFFERS];
                                                     
/* Audio Codec playback buffer data */
static u8 OutboundData0 [NUM_DAC_BUFFERS * AUDIO_CODEC_DAC_BUFFER_SIZE];
static u8 *pOutboundData0 = &OutboundData0[0];
static u8 *pOutboundData1 = &OutboundData0[AUDIO_CODEC_DAC_BUFFER_SIZE];

#define     NUM_AUDIO_ADC_CHANNELS                  2
                                                                                                                                       
/* Define the record buffer size */
#define AUDIO_CODEC_ADC_BUFFER_SIZE                 512                           
                                                     
static ADI_DEV_1D_BUFFER InboundBuffer[NUM_ADC_BUFFERS];

/* Audio Codec record buffer data */
static section("L1_data") u8 InboundData0 [2*AUDIO_CODEC_ADC_BUFFER_SIZE];
static u8 *pInboundData0 = &InboundData0[0];
static u8 *pInboundData1 = &InboundData0[AUDIO_CODEC_ADC_BUFFER_SIZE];

static 	ADI_DEV_1D_BUFFER usbBuffer;
                                                     
#pragma align 4
static section("L1_data") u8 USB_Buffer[PBK_PKT_SIZE];

// If your company already has a USB Vendor ID you should use that here.
// If you are not sure you can contact USB-IF to find out.
#define USB_VID_TEST        0x1234

/* Product string can be user defined */
#define USB_PRODUCT_STRING  "Blackfin USB Audio"

static ADI_DEV_DEVICE_HANDLE adi_DevHandle;										

static void InitSystemServices(void);  								

void ClientCallback( void *AppHandle, u32  Event, void *pArg);

volatile u32 IsDeviceConnected = FALSE;

/* Device ready flag enumerated and connected */
u32 IsDeviceReady   = false;

/* DCB queue size for 10 entries */
#define SIZE_DCB (ADI_DCB_QUEUE_SIZE + (ADI_DCB_ENTRY_SIZE)*10)

/* DCB queue data */
static u8 DCBMgrQueue1[SIZE_DCB];     
static ADI_DCB_HANDLE adi_dcb_QueueHandle = NULL;      

/* playback and Record status flags */
volatile u32    playbackStarted             = false;
volatile u32    recordStarted               = false;
volatile u32    recordBufferProcessed       = true;

volatile u8   Ac97RegAccessComplete;  /* AC'97 Register access complete flag          */

extern  u32 	adi_ssl_Init(void);
extern  u32 	adi_ssl_Terminate(void);

/* HDRC entrypoint */
extern ADI_DEV_PDD_ENTRY_POINT ADI_USBDRC_Entrypoint;

/* Audio codec entrypoint */
extern ADI_DEV_PDD_ENTRY_POINT ADIAD1980EntryPoint;

extern ADI_DEV_DEVICE_HANDLE AudioCodecDriverHandle;

/*********************************************************************

	Function:		failure

	Description:	In case of failure we toggle LEDs.

*********************************************************************/

void failure(void)
{
    while (1)
    {
    }
}

/*********************************************************************

	Function:		ExceptionHandler
					HWErrorHandler

	Description:	We should never get an exception or hardware error,
					but just in case we'll catch them should one ever occur.

*********************************************************************/

static ADI_INT_HANDLER(ExceptionHandler)	/* exception handler */
{
    return(ADI_INT_RESULT_PROCESSED);
}

static ADI_INT_HANDLER(HWErrorHandler)		/* hardware error handler */
{
    return(ADI_INT_RESULT_PROCESSED);
}

section ("L1_code")
/*********************************************************************

	Function:		ClientCallback

*********************************************************************/
static void ClientCallback( void *AppHandle, u32  Event, void *pArg)
{
	u16	codecData;
    u32 i;
    	
    u32 Result = ADI_DEV_RESULT_SUCCESS;

    switch (Event)
    {
        case ADI_USB_EVENT_DATA_TX:
        {
        	recordBufferProcessed = true;
        }
        break;
        
        case ADI_USB_EVENT_DISCONNECT:
        {  
		   	/* Let us know that the device was disconnected */	           
           	IsDeviceConnected = FALSE;
			IsDeviceReady = FALSE;           
        }	
		break;
		        
		case ADI_USB_EVENT_PKT_RCVD_NO_BUFFER:
		{
        	/* Configure the USB buffer */				
   	        usbBuffer.Data = &USB_Buffer;
   	        usbBuffer.ElementCount = PBK_PKT_SIZE;
   	        usbBuffer.ElementWidth = 1;
   	        usbBuffer.ProcessedFlag = FALSE;
   	        usbBuffer.ProcessedElementCount = 0;
   	        usbBuffer.CallbackParameter = &usbBuffer;
   	        usbBuffer.pNext = NULL;
		    
    		/* Pass our buffer down to audio class */
 	    	adi_dev_Control(adi_DevHandle,
                            ADI_USB_AUDIO_CMD_SET_RX_BUFFER,
                            (void*)&usbBuffer);
		}
		break;

		/*  
			Begin codec start/stop event handling for
			playback and record
		*/			
		
		case ADI_USB_AUDIO_EVENT_PLAYBACK_STARTED:
		{
		    /* Notification from class driver, playback started */
		    playbackStarted = true;
		    
		    adi_codec_Start();
		}		    
		break;

		case ADI_USB_AUDIO_EVENT_PLAYBACK_STOPPED:
		{
		    /* Notification from class driver, playback stopped */		    
		    playbackStarted = false;
		    
			adi_codec_Stop();
			
			/* Flush the playback buffers */
            for (i = 0; i < OutboundBuffer[0].ElementCount/sizeof(u32); i++)
            {
                 ((u32*)OutboundBuffer[0].Data)[i] = (u32)0;
            }

			/* Flush the playback buffers */
            for (i = 0; i < OutboundBuffer[1].ElementCount/sizeof(u32); i++)
            {
                 ((u32*)OutboundBuffer[1].Data)[i] = (u32)0;
            }
		}
		break;

		case ADI_USB_AUDIO_EVENT_RECORD_STARTED:
		{
		    /* Notification from class driver, record started */		    
		    recordStarted = true;
		    
		    adi_codec_Start();
		}
		break;

		case ADI_USB_AUDIO_EVENT_RECORD_STOPPED:
		{
		    /* Notification from class driver, record stopped */		    
		    recordStarted = false;
		    
			adi_codec_Stop();
        			    
           	/* Zero out the record buffer */
       	    for (i = 0; i < sizeof(InboundData0);i++)
       	    {
           	    pInboundData0[i] = 0;
       	    }           	
		}
		break;
		
		/*  
			Begin handling of codec register callback events from
			the audio class driver
		*/			
		
		/* Set the Master Channel volume */
		case ADI_USB_AUDIO_EVENT_SET_MASTER_VOLUME:
		{
			codecData = (u16)pArg;
			adi_codec_SetMasterVolume(codecData);
		}
		break;

		/* Mute the MASTER channel */
		case ADI_USB_AUDIO_EVENT_SET_MASTER_MUTE:
		{
			codecData = (u16)pArg;
			adi_codec_SetMasterVolume(codecData);
		}
		break;

		/* Set LINE-IN volume */
		case ADI_USB_AUDIO_EVENT_SET_LINEIN_VOLUME:
		{
			codecData = (u16)pArg;
			adi_codec_SetLineInVolume(codecData);
		}
		break;

		/* Mute LINE-IN channel */
		case ADI_USB_AUDIO_EVENT_SET_LINEIN_MUTE:
		{
			
			codecData = (u16)pArg;
			adi_codec_SetLineInMute(codecData);
		}
		break;

		/* Set MIC volume */		
		case ADI_USB_AUDIO_EVENT_SET_MIC_VOLUME:
		{
			codecData = (u16)pArg;
			adi_codec_SetMicVolume(codecData);
		}
		break;

		/* Mute MIC channel */		
		case ADI_USB_AUDIO_EVENT_SET_MIC_MUTE:
		{
			codecData = (u16)pArg;
			adi_codec_SetMicMute(codecData);
		}
		break;
		
		/* Enable LINE-IN for record */
		case ADI_USB_AUDIO_EVENT_SET_LINEIN_SELECT:
		{
			adi_codec_SetLineIn_Select();
		}
		break;

		/* Enable MIC for record */
		case ADI_USB_AUDIO_EVENT_SET_MIC_SELECT:
		{
			adi_codec_SetMicSelect();
   		}
		break;
		
		/* Set the record channel gain */
		case ADI_USB_AUDIO_EVENT_SET_RECORD_MASTER_GAIN:
		{
			codecData = (u16)pArg;
			adi_codec_SetRecord_Master_Gain(codecData);
  		}
		break;
		
		/* Return the current Master Volume setting */
		case ADI_USB_AUDIO_EVENT_GET_MASTER_VOLUME:
		{
		
			codecData = adi_codec_Get_Master_Volume();
				
			/* Send control data to host */                      	 
   			adi_dev_Control(adi_DevHandle,
            		        ADI_USB_AUDIO_CMD_RETURN_CTL_DATA_2,
                    		(void*)codecData);	
		}
		break;

		/* Return the current LINE-IN Volume setting */
		case ADI_USB_AUDIO_EVENT_GET_LINEIN_VOLUME:
		{

			codecData = adi_codec_Get_LineIn_Volume();	
					
			/* Send control data to host */                      	 
   	    	adi_dev_Control(adi_DevHandle,
                            ADI_USB_AUDIO_CMD_RETURN_CTL_DATA_2,
                            (void*)codecData);				
		}
		break;
		
		/* Return the current MIC Volume setting */
		case ADI_USB_AUDIO_EVENT_GET_MIC_VOLUME:
		{
			codecData = adi_codec_Get_Mic_Volume();
			
			/* Send control data to host */                      	 
   	    	adi_dev_Control(adi_DevHandle,
                            ADI_USB_AUDIO_CMD_RETURN_CTL_DATA_2,
                            (void*)codecData);				
		}
		break;
					
		default:
        break;
    }
}

section ("L1_code")
/*********************************************************************

	Function:		AudioCodecCallback

*********************************************************************/
static void AudioCodecCallback( void *AppHandle, u32  Event, void *pArg)
{
    u32 i;
    u32 *pCodecData, *pAppData, bufferIndex, bufferType;
    
    ADI_DEV_1D_BUFFER *pBuffer = (ADI_DEV_1D_BUFFER *)pArg;
    
    /* Get the processed buffer type 0 = pbk, 1 = rec */
    bufferType  = (((u32)pBuffer->pAdditionalInfo & 0x10) >> 4);
    
    /* Get the processed buffer index */
    bufferIndex = ((u32)pBuffer->pAdditionalInfo & 0x0f);
    
	switch (Event)
	{
	    /* AC'97 register access complete */
	    case ADI_AC97_EVENT_REGISTER_ACCESS_COMPLETE:
	    {
	        /* Mark as AD1980 register access complete */
	        Ac97RegAccessComplete = TRUE;
	    }
	    break;

		/* CASE (DMA buffer processed) */
		case ADI_DEV_EVENT_BUFFER_PROCESSED:
		{
            /* If this is a record buffer process it */
            if((bufferType == 1) && (recordStarted == true) &&
            	(recordBufferProcessed == true))
            {
				recordBufferProcessed = false;
				            	
	            adi_dev_Write(adi_DevHandle,
                               ADI_DEV_1D,
                               (ADI_DEV_BUFFER *)pBuffer);
                               
            } /* Playback buffer and playback is started */
            else if((bufferType == 0) && (playbackStarted == true))
            {
                /*
                   Zero out the buffer before we pass it back to the 
                   Audio class driver 
                */
                for (i = 0; i < pBuffer->ElementCount/sizeof(u32); i++)
                {
                     ((u32*)pBuffer->Data)[i] = (u32)0;
                }
                
                /* 
                    The buffer was processed by the codec put it 
                    back on the list
                */  
                adi_dev_Read(adi_DevHandle,
                             ADI_DEV_1D,
                             (ADI_DEV_BUFFER *)pBuffer);
            }
   		}
		break;
		
		/* CASE (DMA error) */
		case ADI_DEV_EVENT_DMA_ERROR_INTERRUPT:
		{
	        printf("DMA error\n");
		}
		break;
		
		default:
		{
            printf ("AudioCodecCallback: Unrecognised Event code: 0x%08X\n",Event); 
		}
        break;
	}	
}

void main(void)
{
    ADI_DEV_MANAGER_HANDLE DeviceManagerHandle;	/* DevMgr handle */
    ADI_DMA_MANAGER_HANDLE DMAHandle;			/* DMAMgr handle */
    PDEVICE_DESCRIPTOR pDevDesc;				/* device descriptor ptr */
    u16 i;      								/* index */
	ADI_DEV_PDD_HANDLE PeripheralDevHandle;
	

	u32 Result = ADI_DEV_RESULT_SUCCESS;		/* result */
    bool bRunning = TRUE;						/* keep running flag */

	/* for rev 0.0 or 0.1 we need to calibrate the USB analog PHY, other revs may not need
	   this or they may use different values altogether */
	if ((0x00 == ((*pDSPID) & 0xFF)) ||
		(0x01 == ((*pDSPID) & 0xFF)))
	{
		/* clear it to make sure USB will work after booting */
		*pUSB_APHY_CNTRL = 0;

		/* setup calib register */
		*pUSB_APHY_CALIB = 0x5411;
		ssync();
	}        
	
    /* Initialize system services */
    adi_ssl_Init();  

	/* Open a DCB queue for Audio Callbacks */
    adi_dcb_Open(
					14, 
    				&DCBMgrQueue1[ADI_DCB_QUEUE_SIZE], 
					(ADI_DCB_ENTRY_SIZE)*10, 
					&Result, 
					&adi_dcb_QueueHandle
					);
        
   /* Initialize USB Core 	 */
    adi_usb_CoreInit((void*)&Result);
     
    /* Open the Controller driver */
    Result = adi_dev_Open(	adi_dev_ManagerHandle, 					/* DevMgr handle */
							&ADI_USBDRC_Entrypoint,					/* pdd entry point */
							0,                          			/* device instance */
							(void*)1,								/* client handle callback identifier */
							&PeripheralDevHandle,					/* device handle */
							ADI_DEV_DIRECTION_BIDIRECTIONAL,		/* data direction for this device */
							NULL,									/* handle to DmaMgr for this device */
							NULL,									/* handle to deferred callback service */
							ClientCallback);                		/* callback function */
							
    if (Result != ADI_DEV_RESULT_SUCCESS)
        failure();
       
    /* Open the Audio Class driver */
    Result = adi_dev_Open(	adi_dev_ManagerHandle,					/* DevMgr handle */
                            &ADI_USB_Device_AudioClass_Entrypoint,  /* pdd entry point */
                            0,										/* device instance */
                            (void*)0x1,								/* client handle callback identifier */
                            &adi_DevHandle,							/* DevMgr handle for this device */
                            ADI_DEV_DIRECTION_BIDIRECTIONAL,		/* data direction for this device */
                            adi_dma_ManagerHandle,					/* handle to DmaMgr for this device */
                            NULL,									/* handle to deferred callback service */
                            ClientCallback);    	    			/* client's callback function */
                            
    if (Result != ADI_DEV_RESULT_SUCCESS)
        failure();

    /* Initialize the Audio Codec */
    Result = adi_codec_Init( 
                                &OutboundData0[0], 
                                AUDIO_CODEC_DAC_BUFFER_SIZE, 
                                &InboundData0[0], 
                                AUDIO_CODEC_ADC_BUFFER_SIZE,
                                NULL, 
                                AudioCodecCallback
                           );
                           
    if (Result != ADI_DEV_RESULT_SUCCESS)
        failure();                           
                               
	/* Give the controller handle to the Audio class driver */
    Result = adi_dev_Control(adi_DevHandle,
    						 ADI_USB_CMD_CLASS_SET_CONTROLLER_HANDLE,
    						 (void*)PeripheralDevHandle);
    
	if (Result != ADI_DEV_RESULT_SUCCESS)
		failure();

	/* Set the number of playback channels we want */
	Result = adi_dev_Control(adi_DevHandle,
							 ADI_USB_AUDIO_CMD_SET_PLAYBACK_CHANNELS,
							 (void*)NUM_AUDIO_DAC_CHANNELS);
	
	if (Result != ADI_DEV_RESULT_SUCCESS)
		failure();
		
	/* Set the initial record select to Microphone */
	Result = adi_dev_Control(adi_DevHandle,
							 ADI_USB_AUDIO_CMD_SET_MIC_RECORD_SELECT,
							 (void*)0);
	
	if (Result != ADI_DEV_RESULT_SUCCESS)
		failure();
		
	/* Configure the Audio class driver */
	Result = adi_dev_Control(adi_DevHandle,
							 ADI_USB_CMD_CLASS_CONFIGURE,
							 (void*)0);
	
	if (Result != ADI_DEV_RESULT_SUCCESS)
		failure();
		
	/* Set the Vendor ID */
    Result = adi_dev_Control(adi_DevHandle,
                             ADI_USB_AUDIO_CMD_SET_VID,
                             USB_VID_TEST);
    if (Result != ADI_DEV_RESULT_SUCCESS)
        failure();
        
	/* Set the Product String */
    Result = adi_dev_Control(adi_DevHandle,
                             ADI_USB_AUDIO_CMD_SET_PRODUCT_STRING,
                             USB_PRODUCT_STRING);
    if (Result != ADI_DEV_RESULT_SUCCESS)
        failure();        
	          	
    /* Set data flow  */
    Result = adi_dev_Control(adi_DevHandle,
                             ADI_DEV_CMD_SET_DATAFLOW_METHOD,
                             (void*)ADI_DEV_MODE_CHAINED);
                             
    if (Result != ADI_INT_RESULT_SUCCESS)
        failure();
        
	/* Enable data flow */
    Result = adi_dev_Control(adi_DevHandle,
                             ADI_DEV_CMD_SET_DATAFLOW,
                             (void*)TRUE);
    if (Result != ADI_INT_RESULT_SUCCESS)
        failure();

    /****************** Send Codec out working Buffer to Audio class driver **********************/

   	/* Outbound 1D buffers */
   	UsbOutboundBuffer[0].Data                = (void *)pOutboundData0;
   	UsbOutboundBuffer[0].ElementCount        = sizeof(OutboundData0)/2;
   	UsbOutboundBuffer[0].ElementWidth        = 1;
  	UsbOutboundBuffer[0].CallbackParameter   = &UsbOutboundBuffer[0];   /* enable callback on completion of this buffer */
	UsbOutboundBuffer[0].ProcessedFlag       = FALSE;
  	UsbOutboundBuffer[0].pNext               = NULL;
    	
  	/* Mark this as playback buffer number 0 */
   	UsbOutboundBuffer[0].pAdditionalInfo     = (void *)0;

	UsbOutboundBuffer[1].Data                = (void *)pOutboundData1;
	UsbOutboundBuffer[1].ElementCount        = sizeof(OutboundData0)/2;
	UsbOutboundBuffer[1].ElementWidth        = 1;
  	UsbOutboundBuffer[1].CallbackParameter   = &UsbOutboundBuffer[1];   /* enable callback on completion of this buffer */
	UsbOutboundBuffer[1].ProcessedFlag       = FALSE;
  	UsbOutboundBuffer[1].pNext               = NULL;
   	
   	/* Mark this as playback buffer number 1 */    	
   	UsbOutboundBuffer[1].pAdditionalInfo     = (void *)1;
    
    adi_dev_Read(adi_DevHandle,
                ADI_DEV_1D,
                (ADI_DEV_BUFFER *)&UsbOutboundBuffer[0]);
                
    adi_dev_Read(adi_DevHandle,
                ADI_DEV_1D,
                (ADI_DEV_BUFFER *)&UsbOutboundBuffer[1]);
                
    /****************** USB Buffer preparation **********************/
                        
	/* Configure the USB buffer */				
   	usbBuffer.Data = &USB_Buffer;
  	usbBuffer.ElementCount = PBK_PKT_SIZE;
   	usbBuffer.ElementWidth = 1;
   	usbBuffer.ProcessedFlag = FALSE;
   	usbBuffer.ProcessedElementCount = 0;
   	usbBuffer.CallbackParameter = &usbBuffer;
   	usbBuffer.pNext = NULL;

    /****************************************************************/
   	 	            		
    /* Enable the Audio class driver */
    Result = adi_dev_Control(PeripheralDevHandle,
                             ADI_USB_CMD_ENABLE_USB,
                            (void*)0);
    if (Result != ADI_INT_RESULT_SUCCESS)
            failure();

	while(bRunning)
	{
	              
		/* Wait until the device is configured by the host */              
    	while(IsDeviceReady == FALSE)
    	{
	    	adi_dev_Control(adi_DevHandle,
                            ADI_USB_AUDIO_CMD_IS_DEVICE_CONFIGURED,
                            &IsDeviceReady); 
 		}

 		IsDeviceConnected = TRUE;
 		
		/* Pass our buffer down to audio class */
 		Result = adi_dev_Control(adi_DevHandle,
                          ADI_USB_AUDIO_CMD_SET_RX_BUFFER,
                          (void*)&usbBuffer);
                             
   		if (Result != ADI_INT_RESULT_SUCCESS)
       		failure();
       		
	    while (IsDeviceConnected == TRUE)
    	{
        	;   
    	}
	}
		
    /* Close the device */
    adi_ssl_Terminate();
}

