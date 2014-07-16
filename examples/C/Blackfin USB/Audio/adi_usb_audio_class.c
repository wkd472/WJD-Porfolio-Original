/*********************************************************************************

Copyright(c) 2008 Analog Devices, Inc. All Rights Reserved.

This software is proprietary and confidential.  By using this software you agree
to the terms of the associated Analog Devices License Agreement.

Description:

    This is the driver source code for the USB Audio
    Class Driver (Device Mode).
 
By: Wayne Dupont

*********************************************************************************/

#include <services/services.h>      			
#include <drivers/adi_dev.h>        

#include <drivers/usb/usb_core/adi_usb_objects.h>       
#include <drivers/usb/usb_core/adi_usb_core.h>          
#include <drivers/usb/usb_core/adi_usb_ids.h>
#include <drivers/usb/controller/otg/adi/hdrc/adi_usb_hdrc.h>
#include <drivers\usb\usb_core\adi_usb_debug.h>

#include <adi_usb_audio_class.h> 

#include <string.h>
#include <stdio.h>

/*********************************************************************/

/* USB Audio command block handlers */
bool AudioGetRequest (u8 bRequest, u8 wIndexHi, u16 wValue, u16 wLength, u16 *pNumBytesToTransfer, u8 *buffer);
bool AudioSetRequest (u8 bRequest, u8 wIndexHi, u16 wValue, u16 wLength, u8 *buffer);

/* Endpoint Zero callback function */
void EndpointZeroCallback( void *Handle, u32  Event, void *pArg);       

/* Return data from Audio control requests */
void ReturnAudioControlData (u16 bytesToReturn);

/* Get data from Audio control requests */
void GetAudioControlData (u16 bytesToReturn);

/* Audio control return buffer */
static ADI_DEV_1D_BUFFER   AudioControlBuffer;
static u8  AudioControlData[5 * sizeof(u8)];

static u32 gSetupRequest = true;
static u16 wValidAudioPkt = false;
static u8  bMuteControl = false;
static u8  bVolumeControl = false;

/*********************************************************************/

#include <cplb.h>
extern  int     __cplb_ctrl;

/*********************************************************************
    Globals
*********************************************************************/

/* Pointers to endpoint information */
USB_EP_INFO *adi_usb_audio_pOUTEPInfo;
USB_EP_INFO *adi_usb_audio_pINEPInfo;

ADI_DEV_DEVICE_HANDLE   adi_usb_audio_PeripheralDevHandle;

ADI_DEV_MANAGER_HANDLE adi_usb_audio_dev_handle;
ADI_DMA_MANAGER_HANDLE adi_usb_audio_dma_handle;
ADI_DCB_HANDLE         adi_usb_audio_dcb_handle;

/* Initialize the device data structure */
AUDIO_STREAMING_DEV_DATA adi_usb_audio_def = {0};

extern void EndpointCompleteCallback(void *Handle, u32  Event, void *pArg);

/*********************************************************************
*
*   Function:       DevAudioConfigure
*
*   Description:    Configures the Audio device
*
*********************************************************************/
static u32 DevAudioConfigure(void)
{
    unsigned int Result;
    int i;
      
   OBJECT_INFO *pOInf;
   PINPUT_TERMINAL_DESCRIPTOR          			pOutInputTD;
   POUTPUT_TERMINAL_DESCRIPTOR         			pOutOutputTD;
   PINPUT_TERMINAL_DESCRIPTOR          			pInInputTD;
   POUTPUT_TERMINAL_DESCRIPTOR         			pInOutputTD;
   PFEATURE_UNIT_DESCRIPTOR            			pFeatrureUD;
   PENDPOINT_DESCRIPTOR                			pEndpointD;
   PCS_SPECIFIC_AS_INTERFACE_DESCRIPTOR         pFormatTypeD;
   PTYPE_1_FORMAT_TYPE_DESCRIPTOR               pType1FormatD;
   PCONFIGURATION_DESCRIPTOR           			pConfigD;
   PINTERFACE_DESCRIPTOR               			pInterfaceD;
   PINTERFACE_OBJECT                   			pInterfaceO;
   PCS_SPECIFIC_AC_DESCRIPTOR          			pCSpecACD;
   PUSB_EP_INFO                        			pEpInfo;
   LOGICAL_EP_INFO                     			LogEpInfo;
   PAS_ISOCHRONOUS_EP_DESCRIPTOR       			pASEndpointD;
   PCS_SPECIFIC_AS_ISOCHRONOUS_EP_DESCRIPTOR    pAudioEpD;
   PAC_EP_DESCRIPTOR_EXT           				pAudioEpDExt;
       
   /* Get a pointer to our audio device structure */
   AUDIO_STREAMING_DEV_DATA *pDevice = &adi_usb_audio_def;

   /* Assume success */
   Result = ADI_DEV_RESULT_SUCCESS;

   /* Set Peripheral driver handle */
   adi_usb_SetPhysicalDriverHandle(pDevice->PeripheralDevHandle);

   /* Set USB to Device mode */
   adi_usb_SetDeviceMode(MODE_DEVICE);

   /* Get the current Device ID. Once the driver is opened its
   *  suppose to create the device. If the device is already opened then
   *  it must return the same device ID.
   */ 
   adi_dev_Control(pDevice->PeripheralDevHandle,
                    ADI_USB_CMD_GET_DEVICE_ID,
                    (void*)&pDevice->DeviceID);

  /*****************************************************
  *
  * Create Audio Configuration Descriptor Object
  *
  */
  pOInf = &pDevice->ConfigObject;
  pOInf->ID = adi_usb_CreateConfiguration((PCONFIG_OBJECT*)&pOInf->pObj);
  
  /* Setup the callback for Endpoint 0 */
  ((PCONFIG_OBJECT)pOInf->pObj)->EpZeroCallback = (ADI_DCB_CALLBACK_FN)EndpointZeroCallback;
    
  /* Attach configuration to the device object */
  adi_usb_AttachConfiguration(pDevice->DeviceID,pOInf->ID);

  /* Fill  configuration descriptor */
  pConfigD = ((PCONFIG_OBJECT)pOInf->pObj)->pConfigDesc; 
    
  pConfigD->bLength             = CONFIGURATION_DESCRIPTOR_LEN;     /* Configuration Descriptor Length */
  pConfigD->bDescriptorType     = TYPE_CONFIGURATION_DESCRIPTOR;    /* Type */
  pConfigD->wTotalLength      	= TOTAL_CONFIG_DESCRIPTOR_LEN;      /* Total Config Length */
  pConfigD->bNumInterfaces      = AUDIO_NUM_TOTAL_INTERFACES;       /* Total number of Interfaces */
  pConfigD->bConfigurationValue = 0x01;                             /* Configuration Value */
  pConfigD->bIConfiguration     = 0x00;                             /* Configuration String */
  pConfigD->bAttributes         = 0xC0;                             /* Self Powered */
  pConfigD->bMaxPower           = 0x32;                             /* Max Power 100 mA*/                   
  
  /*******************************************************
  *
  * Standard Audio Control Interface @ Interface #0 
  * Create Audio Control Interface Object and configure it.
  *
  */
  pOInf = &pDevice->InterfaceObjects[AUDIO_CONTROL];  
  pOInf->ID = adi_usb_CreateInterface((PINTERFACE_OBJECT*)&pOInf->pObj);
  
  /* Attach Audio control Interface object to the Configuration object */
  adi_usb_AttachInterface(pDevice->ConfigObject.ID,pOInf->ID);

  pInterfaceD = ((PINTERFACE_OBJECT)pOInf->pObj)->pInterfaceDesc; 
    
  pInterfaceD->bLength              = INTERFACE_DESCRIPTOR_LEN;
  pInterfaceD->bDescriptorType      = TYPE_INTERFACE_DESCRIPTOR;
  pInterfaceD->bInterfaceNumber     = pOInf->ID;
  pInterfaceD->bAlternateSetting    = ALTSETTING_FOR_AUDIOCONTROL; 
  pInterfaceD->bNumEndpoints        = 0; 
  pInterfaceD->bInterfaceClass      = USB_AUDIO_CLASS_CODE;
  pInterfaceD->bInterfaceSubClass   = USB_AUDIO_SUBCLASS_AUDIO_CONTROL;
  pInterfaceD->bInterfaceProtocol   = USB_AUDIO_PROTOCOL_NONE;
  pInterfaceD->bIInterface          = 0; 

  /* Add the Device Specific interfaces */
  ((PINTERFACE_OBJECT)pOInf->pObj)->pIntSpecificObj = &pDevice->IFaceClassSpecificObj[0];
  
  /*******************************************************
  *
  * Class Specific Audio Control Header Descriptor 
  * Create Audio Control Interface Object and configure it.
  *
  */
  pCSpecACD = &pDevice->acSpecifcInfo.csACD;

  pCSpecACD->bLength                = AUDIO_CONTROL_INTERFACE_DESCRIPTOR_LEN; 
  pCSpecACD->bDescriptorType        = USB_AUDIO_DESCRIPTOR_INTERFACE; 
  pCSpecACD->bDescriptorSubtype     = USB_AC_DESCRIPTOR_SUBTYPE_HEADER;       
  pCSpecACD->bcdADC_lo  			= LOBYTE(USB_AUDIO_SPEC_1_0);             
  pCSpecACD->bcdADC_hi      		= HIBYTE(USB_AUDIO_SPEC_1_0);             
  pCSpecACD->TotalLength_lo     	= LOBYTE(TOTAL_AUDIO_CONTROL_DESCRIPTOR_LEN);       
  pCSpecACD->TotalLength_hi   		= HIBYTE(TOTAL_AUDIO_CONTROL_DESCRIPTOR_LEN);       
  pCSpecACD->bInCollection      	= 0x01;                       
  pCSpecACD->baInterfaceNr[0]       = 0x01;   
  
  pDevice->IFaceClassSpecificObj[0].length = AUDIO_CONTROL_INTERFACE_DESCRIPTOR_LEN;
  pDevice->IFaceClassSpecificObj[0].pIntSpecificData = pCSpecACD;
  pDevice->IFaceClassSpecificObj[0].pNext = &pDevice->IFaceClassSpecificObj[1];
  
  /*******************************************************
  *
  *  Initialize Line Out - Input Terminal descriptor audio data arrives here
  *  via the USB OUT endpoint.
  *
  */
  pOutInputTD =  &pDevice->acSpecifcInfo.out_input_TD;
  
  pOutInputTD->bLength            = AUDIO_CONTROL_OUT_INPUT_TERMINAL_DESCRIPTOR_LEN; 
  pOutInputTD->bDescriptorType    = USB_AUDIO_CS_INTERFACE; 
  pOutInputTD->bDescriptorSubtype = USB_AC_DESCRIPTOR_SUBTYPE_INPUT_TERMINAL;
  pOutInputTD->bTerminalID        = AUDIO_TERMID_USBOUT; 
  pOutInputTD->wTerminalType      = USB_AUDIO_TERMINAL_TYPE_STREAMING; 
  pOutInputTD->bAssocTerminal     = 0; 
  pOutInputTD->bNrChannels        = USB_AUDIO_NUM_PLAYBACK_CHANNELS;  
  pOutInputTD->wChannelConfig     = USB_AUDIO_CHANNEL_CONFIG_BITMAP(USB_AUDIO_NUM_PLAYBACK_CHANNELS);  
  pOutInputTD->iChannelNames      = 0;
  pOutInputTD->iTerminal          = 0;

  /* Add the class specific information */  
  pDevice->IFaceClassSpecificObj[1].length = AUDIO_CONTROL_OUT_INPUT_TERMINAL_DESCRIPTOR_LEN;
  pDevice->IFaceClassSpecificObj[1].pIntSpecificData = pOutInputTD;
  pDevice->IFaceClassSpecificObj[1].pNext = &pDevice->IFaceClassSpecificObj[2];
          
  /*******************************************************
  *
  *  Initialize Feature unit descriptor. Number of bytes per
  *  control is assumed to be 2. The logic supports any number
  *  of logical channels but if application changes the number
  *  of bytes to represent a channel then the code needs to be 
  *  changed.
  *
  */
  pFeatrureUD = &pDevice->acSpecifcInfo.featureUD;
  
  pFeatrureUD->bLength      			=  AUDIO_CONTROL_FEATURE_UNIT_DESCRIPTOR_LEN;
  pFeatrureUD->bDescriptorType          =  USB_AUDIO_CS_INTERFACE; 
  pFeatrureUD->bDescriptorSubtype       =  USB_AC_DESCRIPTOR_SUBTYPE_FEATURE_UNIT; 
  pFeatrureUD->bUnitID    				=  AUDIO_UNITID_VOLUME_OUT; 
  pFeatrureUD->bSourceID  				=  AUDIO_TERMID_USBOUT; 
  pFeatrureUD->bControlSize     	    =  0x02;

#if 1 /* Two Channel device */
  pFeatrureUD->bmaControls[0]   	=  0x03;  /* Channel 0 master  MUTE and VOLUME */
  pFeatrureUD->bmaControls[1]   	=  0x00; 
  pFeatrureUD->bmaControls[2]   	=  0x00;  /* Channel 1 MUTE and VOLUME */
  pFeatrureUD->bmaControls[3]   	=  0x00; 
  pFeatrureUD->bmaControls[4]   	=  0x00;  /* Channel 2 MUTE and VOLUME */
  pFeatrureUD->bmaControls[5]   	=  0x00; 
#else /* Six Channel device */
  pFeatrureUD->bmaControls[0]   	=  0x03;  /* Channel 0 master  MUTE */
  pFeatrureUD->bmaControls[1]   	=  0x00; 
  pFeatrureUD->bmaControls[2]   	=  0x03;  /* Channel 1 Volume */
  pFeatrureUD->bmaControls[3]   	=  0x00; 
  pFeatrureUD->bmaControls[4]   	=  0x03;  /* Channel 2 Volume */
  pFeatrureUD->bmaControls[5]   	=  0x00; 
  pFeatrureUD->bmaControls[6]   	=  0x03;  /* Channel 3 Volume */
  pFeatrureUD->bmaControls[7]   	=  0x00; 
  pFeatrureUD->bmaControls[8]   	=  0x03;  /* Channel 4 Volume */
  pFeatrureUD->bmaControls[9]   	=  0x00; 
  pFeatrureUD->bmaControls[10]  	=  0x03;  /* Channel 5 Volume */
  pFeatrureUD->bmaControls[11]  	=  0x00; 
  pFeatrureUD->bmaControls[12]  	=  0x03;  /* Channel 6 Volume */
  pFeatrureUD->bmaControls[13]  	=  0x00; 
#endif    
  pFeatrureUD->iFeature       		=  0x00;  /* No string descriptor */

  /* Add the class specific information */  
  pDevice->IFaceClassSpecificObj[2].length = AUDIO_CONTROL_FEATURE_UNIT_DESCRIPTOR_LEN;
  pDevice->IFaceClassSpecificObj[2].pIntSpecificData = pFeatrureUD;
  pDevice->IFaceClassSpecificObj[2].pNext = &pDevice->IFaceClassSpecificObj[3];
  
  /*******************************************************
  *
  *  Initialize Line Out - Output Terminal descriptor 
  *  Audio data exits through this terminal
  *
  */
  pOutOutputTD =  &pDevice->acSpecifcInfo.out_output_TD;
  
  pOutOutputTD->bLength 				= AUDIO_CONTROL_OUT_OUTPUT_TERMINAL_DESCRIPTOR_LEN; 
  pOutOutputTD->bDescriptorType 		= USB_AUDIO_DESCRIPTOR_INTERFACE; 
  pOutOutputTD->bDescriptorSubtype 	    = USB_AC_DESCRIPTOR_SUBTYPE_OUTPUT_TERMINAL; 
  pOutOutputTD->bTerminalID 			= AUDIO_TERMID_LINEOUT;
  pOutOutputTD->wTerminalType 			= USB_AUDIO_OUTPUT_TERMINALTYPE_SPEAKER; 
  pOutOutputTD->bAssocTerminal 			= 0; 
  pOutOutputTD->bSourceID 				= AUDIO_UNITID_VOLUME_OUT;  
  pOutOutputTD->iTerminal 				= 0;

  /* Add the class specific information */  
  pDevice->IFaceClassSpecificObj[3].length = AUDIO_CONTROL_OUT_OUTPUT_TERMINAL_DESCRIPTOR_LEN;
  pDevice->IFaceClassSpecificObj[3].pIntSpecificData = pOutOutputTD;
  pDevice->IFaceClassSpecificObj[3].pNext = NULL;

#if 0 /* NOTE : Do not add recording inout/output terminals etc. at this time */  

  /*******************************************************
  *
  *  Initialize Line In - Input Terminal descriptor 
  *
  */
  pInInputTD =  &pDevice->acSpecifcInfo.in_input_TD;
  
  pInInputTD->bLength           	= AUDIO_CONTROL_IN_INPUT_TERMINAL_DESCRIPTOR_LEN; 
  pInInputTD->bDescriptorType   	= USB_AUDIO_CS_INTERFACE; 
  pInInputTD->bDescriptorSubtype    = USB_AC_DESCRIPTOR_SUBTYPE_INPUT_TERMINAL;
  pInInputTD->bTerminalID       	= AUDIO_TERMID_LINEIN; 
  pInInputTD->wTerminalType     	= USB_EXTERNAL_TERMINALTYPE_LINE_CONNECTOR; 
  pInInputTD->bAssocTerminal    	= 0; 
  pInInputTD->bNrChannels       	= NUM_CHANNELS_RECORDING;  
  pInInputTD->wChannelConfig    	= CHANNELCONFIG_RECORDING;  
  pInInputTD->iChannelNames     	= 0;
  pInInputTD->iTerminal         	= 0;
  
  /* Add the class specific information */  
  pDevice->IFaceClassSpecificObj[4].length = AUDIO_CONTROL_IN_INPUT_TERMINAL_DESCRIPTOR_LEN;
  pDevice->IFaceClassSpecificObj[4].pIntSpecificData = pInInputTD;
  pDevice->IFaceClassSpecificObj[4].pNext = &pDevice->IFaceClassSpecificObj[5];
  
  /*******************************************************
  *
  *  Initialize Line In - Output Terminal descriptor this
  *  is routed to the USB IN endpoint
  *
  */
  pInOutputTD =  &pDevice->acSpecifcInfo.in_output_TD;
  
  pInOutputTD->bLength				= AUDIO_CONTROL_IN_OUTPUT_TERMINAL_DESCRIPTOR_LEN; 
  pInOutputTD->bDescriptorType 		= USB_AUDIO_DESCRIPTOR_INTERFACE; 
  pInOutputTD->bDescriptorSubtype 	= USB_AC_DESCRIPTOR_SUBTYPE_OUTPUT_TERMINAL; 
  pInOutputTD->bTerminalID 			= AUDIO_TERMID_USBIN;
  pInOutputTD->wTerminalType 		= USB_AUDIO_TERMINAL_TYPE_STREAMING; 
  pInOutputTD->bAssocTerminal 		= 0; 
  pInOutputTD->bSourceID 			= AUDIO_TERMID_LINEIN;  
  pInOutputTD->iTerminal 			= 0;

  /* Add the class specific information */  
  pDevice->IFaceClassSpecificObj[5].length = AUDIO_CONTROL_IN_OUTPUT_TERMINAL_DESCRIPTOR_LEN;
  pDevice->IFaceClassSpecificObj[5].pIntSpecificData = pInOutputTD;
  pDevice->IFaceClassSpecificObj[5].pNext = NULL;

#endif
      
  /****************************************************************
  *
  * Audio Streaming Interface (USB OUT) configuration, Idle   
  *
  */
  pOInf = &pDevice->InterfaceObjects[AUDIO_ALT_STREAM_OUT];  

  pOInf->ID = adi_usb_CreateInterface((PINTERFACE_OBJECT*)&pOInf->pObj);
  
  /* Attach Audio control Interface object to the Configuration object */
  adi_usb_AttachInterface(pDevice->ConfigObject.ID,pOInf->ID);
  
  pInterfaceD = ((PINTERFACE_OBJECT)pOInf->pObj)->pInterfaceDesc;   
      
  pInterfaceD->bLength            = INTERFACE_DESCRIPTOR_LEN;
  pInterfaceD->bDescriptorType    = TYPE_INTERFACE_DESCRIPTOR;
  pInterfaceD->bInterfaceNumber   = INTERFACE_NUM_FOR_AUDIO_STREAMING;
  pInterfaceD->bAlternateSetting  = ALT_INTERFACE_NUM_FOR_AUDIO_STREAMING_IDLE;
  pInterfaceD->bNumEndpoints      = 0;
  pInterfaceD->bInterfaceClass    = USB_AUDIO_CLASS_CODE;
  pInterfaceD->bInterfaceSubClass = USB_AUDIO_SUBCLASS_AUDIO_STREAMING;
  pInterfaceD->bInterfaceProtocol = USB_AUDIO_PROTOCOL_NONE;
  pInterfaceD->bIInterface        = 0;  
  
  /****************************************************************
  *
  * Audio Streaming Interface (USB OUT) configuration, 48K Stereo
  *
  */
  pOInf = &pDevice->InterfaceObjects[AUDIO_STREAM_OUT];  

  pOInf->ID = adi_usb_CreateInterface((PINTERFACE_OBJECT*)&pOInf->pObj);
  
  /* Attach Audio control Interface object to the Configuration object */
  adi_usb_AttachInterface(pDevice->ConfigObject.ID,pOInf->ID);

  pInterfaceD = ((PINTERFACE_OBJECT)pOInf->pObj)->pInterfaceDesc; 
     
  pInterfaceD->bLength            = INTERFACE_DESCRIPTOR_LEN;
  pInterfaceD->bDescriptorType    = TYPE_INTERFACE_DESCRIPTOR;
  pInterfaceD->bInterfaceNumber   = INTERFACE_NUM_FOR_AUDIO_STREAMING;
  pInterfaceD->bAlternateSetting  = ALT_INTERFACE_NUM_FOR_AUDIO_STREAMING_48K;
  pInterfaceD->bNumEndpoints      = 1;
  pInterfaceD->bInterfaceClass    = USB_AUDIO_CLASS_CODE;
  pInterfaceD->bInterfaceSubClass = USB_AUDIO_SUBCLASS_AUDIO_STREAMING;
  pInterfaceD->bInterfaceProtocol = USB_AUDIO_PROTOCOL_NONE;
  pInterfaceD->bIInterface        = 0;

  /* Add the Device Specific interfaces */
  ((PINTERFACE_OBJECT)pOInf->pObj)->pIntSpecificObj = &pDevice->IFaceClassSpecificObj[6];
  
  /****************************************************************
  *
  * Create Audio Class specific General Descriptor 
  *
  */
  pFormatTypeD = &pDevice->acSpecifcInfo.formatTypeD;

  pFormatTypeD->bLength       		= AUDIO_STREAMING_GENERAL_DESCRIPTOR_LEN;  
  pFormatTypeD->bDescriptorType   	= USB_AUDIO_DESCRIPTOR_INTERFACE;          
  pFormatTypeD->bDescriptorSubtype  = USB_AS_DESCRIPTOR_SUBTYPE_GENERAL;
  pFormatTypeD->bTerminalLink     	= AUDIO_TERMID_USBOUT;              
  pFormatTypeD->bDelay        		= PLAYBACK_DELAY;                 
  pFormatTypeD->wFormatTag_lo     	= LOBYTE(USB_AUDIO_FORMAT_TAG_PCM);   
  pFormatTypeD->wFormatTag_hi     	= HIBYTE(USB_AUDIO_FORMAT_TAG_PCM);     

  /* Add the class specific information */  
  pDevice->IFaceClassSpecificObj[6].length = AUDIO_STREAMING_GENERAL_DESCRIPTOR_LEN;
  pDevice->IFaceClassSpecificObj[6].pIntSpecificData = pFormatTypeD;
  pDevice->IFaceClassSpecificObj[6].pNext = &pDevice->IFaceClassSpecificObj[7];

  /****************************************************************
  *
  * Create Audio Class specific Type 1 Format Descriptor 
  *
  */
  pType1FormatD = &pDevice->acSpecifcInfo.type1FormatD;
  
  pType1FormatD->bLength        	= AUDIO_STREAMING_TYPE_I_FMT_DESCRIPTOR_LEN;    /* Size of this descriptor */
  pType1FormatD->bDescriptorType    = USB_AUDIO_DESCRIPTOR_INTERFACE;               /* Interface Descriptor Type */
  pType1FormatD->bDEscriptorSubType = USB_AS_DESCRIPTOR_SUBTYPE_FORMAT_TYPE;        /* Descriptor Sub Type */
  pType1FormatD->bFormatType      	= USB_AUDIO_FORMAT_TYPE_I;                      /* Format Type */
  pType1FormatD->bNrChannels      	= USB_AUDIO_NUM_PLAYBACK_CHANNELS;              /* Number of Physical Channels */
  pType1FormatD->bSubFrameSize    	= CONTAINER_SIZE / 8;                           /* Number of bytes in subframe */
  pType1FormatD->bBitResolution   	= BIT_DEPTH;                                    /* Number of bits used */
  pType1FormatD->bSamFreqType     	= USB_AUDIO_SAMPLING_FREQ_DISCREET;             /* How sampling freq can be programmed */
  pType1FormatD->tSampFreq[0]     	= 0x80; /* 0xBB80 = 48000 Hz */                 /* Sampling freq = 48Khz (0x00BB80) (24 bits)*/
  pType1FormatD->tSampFreq[1]       = 0xBB;
  pType1FormatD->tSampFreq[2]     	= 0x00;

  /* Add the class specific information */  
  pDevice->IFaceClassSpecificObj[7].length = AUDIO_STREAMING_TYPE_I_FMT_DESCRIPTOR_LEN;
  pDevice->IFaceClassSpecificObj[7].pIntSpecificData = pType1FormatD;
  pDevice->IFaceClassSpecificObj[7].pNext = NULL;

  /****************************************************************
  *
  * Create Standard Audio Endpoint Endpoint descriptor - Audio Playback
  *
  */
  pOInf = &pDevice->EndPointObjects[AUDIO_STREAM_EP_PLAYBACK];
  
  /* This will determine what EP number we are assigned */
  LogEpInfo.dwMaxEndpointSize = MAX_PACKET_SIZE_PLAYBACK;

  pOInf->ID = adi_usb_CreateEndPoint((PENDPOINT_OBJECT*)&pOInf->pObj,&LogEpInfo);

  pEndpointD = ((PENDPOINT_OBJECT)pOInf->pObj)->pEndpointDesc;

  /* Attach OUT Endpoint Object */

  adi_usb_AttachEndpoint(pDevice->InterfaceObjects[AUDIO_STREAM_OUT].ID,pOInf->ID);
  pEpInfo = (PUSB_EP_INFO)&(((PENDPOINT_OBJECT)pOInf->pObj)->EPInfo);

  /* Set the callback */
  pEpInfo->EpCallback = EndpointCompleteCallback;
  
  adi_usb_audio_pOUTEPInfo = pEpInfo;
  
  pEndpointD->bLength 			=  AUDIO_STREAMING_ENDPOINT_DESCRIPTOR_LEN;
  pEndpointD->bDescriptorType  	=  TYPE_ENDPOINT_DESCRIPTOR;
  pEndpointD->bEndpointAddress 	=  (((u8)EP_DIR_OUT) | ((u8)pOInf->ID));
  pEndpointD->bAttributes      	=  USB_AUDIO_EP_ATTRIB_ADAPTIVE | USB_AUDIO_EP_ATTRIB_ISOCH;
  pEndpointD->wMaxPacketSize   	=  MAX_PACKET_SIZE_PLAYBACK;
  pEndpointD->bInterval        	=  1;
    
  /* Add the endpoint */
  ((PENDPOINT_OBJECT)pOInf->pObj)->pEndpointSpecificObj = &pDevice->EndpointSpecificObj[0];
  
  /****************************************************************
  *
  * Add the Audio specific info to the end of our standard endpoint
  *
  */
  pAudioEpDExt = &pDevice->acSpecifcInfo.audioEpDExt;
    
  pAudioEpDExt->bRefresh        =  0;
  pAudioEpDExt->bSynchAddress   =  0;
    
  /* Add the class specific information */  
  pDevice->EndpointSpecificObj[0].length = 2;
  pDevice->EndpointSpecificObj[0].pEpSpecificData = pAudioEpDExt;
  pDevice->EndpointSpecificObj[0].pNext = &pDevice->EndpointSpecificObj[1];

  /****************************************************************
  *
  * Create Class Specific Audio Endpoint Endpoint descriptor
  *
  */
  pAudioEpD = &pDevice->acSpecifcInfo.audioEpD;
    
  pAudioEpD->bLength       		= AUDIO_STREAMING_ENDPOINT_GENERAL_DESCRIPTOR_LEN;            
  pAudioEpD->bDescriptorType    = USB_AUDIO_DESCRIPTOR_ENDPOINT;    
  pAudioEpD->bDescriptorSubtype = USB_AS_DESCRIPTOR_SUBTYPE_GENERAL; 
  pAudioEpD->bmAttributes     	= 0x00;       
  pAudioEpD->bLockDelayUnits    = 0x00;    
  pAudioEpD->wLockDelay    		= 0x0000;         
  
  /* Just attach the AS Class Specific EP Information */
  pDevice->EndpointSpecificObj[1].length = AUDIO_STREAMING_ENDPOINT_GENERAL_DESCRIPTOR_LEN;
  pDevice->EndpointSpecificObj[1].pEpSpecificData = pAudioEpD;
  pDevice->EndpointSpecificObj[1].pNext = NULL;

  /****************************************************************
  *
  * If needed enable buffers in cache
  *
  */
  if( (__cplb_ctrl & CPLB_ENABLE_DCACHE ) || (__cplb_ctrl & CPLB_ENABLE_DCACHE2))
  {
      adi_dev_Control(pDevice->PeripheralDevHandle,
                      ADI_USB_CMD_BUFFERS_IN_CACHE,
                      (void *)TRUE);
  }
  
  return(Result);
}

/*********************************************************************
*
*   Function:       adi_pdd_Open
*
*   Description:    Opens the Audio Class device
*                   in Device mode
*
*********************************************************************/
static u32 adi_pdd_Open(
    ADI_DEV_MANAGER_HANDLE       ManagerHandle,         /* device manager handle */
    u32                          DeviceNumber,          /* device number */
    ADI_DEV_DEVICE_HANDLE        DeviceHandle,          /* device handle */
    ADI_DEV_PDD_HANDLE           *pPDDHandle,           /* pointer to PDD handle location */
    ADI_DEV_DIRECTION            Direction,             /* data direction */
    void                         *pCriticalRegionArg,   /* critical region imask storage location */
    ADI_DMA_MANAGER_HANDLE       DMAHandle,             /* handle to the DMA manager */
    ADI_DCB_HANDLE               DCBHandle,             /* callback handle */
    ADI_DCB_CALLBACK_FN          DMCallback             /* device manager callback function */
)
{
    u32 i = 0;
    u32 Result      = ADI_DEV_RESULT_SUCCESS;
    adi_usb_audio_dev_handle = ManagerHandle;
    adi_usb_audio_dma_handle = DMAHandle;
    adi_usb_audio_dcb_handle = DCBHandle;
    
    AUDIO_STREAMING_DEV_DATA *pDevice = &adi_usb_audio_def;


    /* Check if the device is already opened */
    if(!pDevice->Open)
    {
        pDevice->Open = true;
        pDevice->DeviceHandle = DeviceHandle;
        pDevice->DCBHandle    = DCBHandle;
        pDevice->DMCallback   = DMCallback;
        pDevice->CriticalData = pCriticalRegionArg;
        pDevice->Direction    = Direction;
    }

    return(Result);
}

/*********************************************************************
*
*   Function:       pddClose
*
*   Description:    Closes the Audio device
*
*********************************************************************/

static u32 adi_pdd_Close(
    ADI_DEV_PDD_HANDLE PDDHandle    /* PDD handle */
)
{
    u32 Result = ADI_DEV_RESULT_SUCCESS;

    AUDIO_STREAMING_DEV_DATA   *pDevice = (AUDIO_STREAMING_DEV_DATA *)&adi_usb_audio_def;

  /* Close down the peripheral driver */
    Result = adi_dev_Close(pDevice->PeripheralDevHandle);

    return(Result);
}

/*********************************************************************
*
*   Function:       pddRead
*
*   Description:
*
*********************************************************************/

static u32 adi_pdd_Read(
    ADI_DEV_PDD_HANDLE PDDHandle,   /* PDD handle */
    ADI_DEV_BUFFER_TYPE BufferType, /* buffer type */
    ADI_DEV_BUFFER *pBuffer         /* pointer to buffer */
)
{
    u32 Result;

    AUDIO_STREAMING_DEV_DATA   *pDevice = (AUDIO_STREAMING_DEV_DATA *)&adi_usb_audio_def;

    ((ADI_DEV_1D_BUFFER*)pBuffer)->Reserved[BUFFER_RSVD_EP_ADDRESS] = pDevice->audio_OUT_EP;

    Result = adi_dev_Read(  pDevice->PeripheralDevHandle,
                            BufferType,
                            pBuffer);
    return(Result);
}

/*********************************************************************
*
*   Function:       pddWrite
*
*   Description:
*
*********************************************************************/

static u32 adi_pdd_Write(
    ADI_DEV_PDD_HANDLE PDDHandle,   /* PDD handle */
    ADI_DEV_BUFFER_TYPE BufferType, /* buffer type */
    ADI_DEV_BUFFER *pBuffer         /* pointer to buffer */
)
{
    AUDIO_STREAMING_DEV_DATA   *pDevice;
    
    pDevice = (AUDIO_STREAMING_DEV_DATA *)&adi_usb_audio_def;

    return (adi_dev_Write(pDevice->PeripheralDevHandle,
                            BufferType,
                            pBuffer));
}

/*********************************************************************
*
*   Function:       EnumerateEndpoints
*
*   Description:
*
*********************************************************************/

static s32 EnumerateEndpoints(ADI_ENUM_ENDPOINT_INFO *pEnumEpInfo, AUDIO_STREAMING_DEV_DATA *pDevice)
{
int i;
unsigned int Result=ADI_DEV_RESULT_SUCCESS;
ENDPOINT_DESCRIPTOR *pEndpointD;
    
ADI_USB_APP_EP_INFO *pEpInfo = pEnumEpInfo->pUsbAppEpInfo;

s32  dwTotalEpEntries = pEnumEpInfo->dwEpTotalEntries;
s32  dwNumEndpoints = sizeof(pDevice->EndPointObjects) / sizeof(OBJECT_INFO);


     /* If supplied memory is not sufficent then we return error */
     if(pEnumEpInfo->dwEpTotalEntries < dwNumEndpoints)
     {
         /* Set the total required entries */
         pEnumEpInfo->dwEpProcessedEntries = dwNumEndpoints;
         Result = ADI_DEV_RESULT_NO_MEMORY;
         return(Result);
     }
     
     for(i=0; i < dwNumEndpoints; i++)
     {
        /* Get the associated endpoint descriptor */
        pEndpointD = ((PENDPOINT_OBJECT)(pDevice->EndPointObjects[i].pObj))->pEndpointDesc;

        /* Get the endpoint ID */
        pEpInfo->dwEndpointID = pDevice->EndPointObjects[i].ID;

        /* Set endpoint direction */
        pEpInfo->eDir = ((pEndpointD->bEndpointAddress >> 7) & 0x1) ? USB_EP_IN : USB_EP_OUT ;

        /* Set the endpoint attributes */
        pEpInfo->bAttributes = pEndpointD->bAttributes;

       pEpInfo++;
     }
     
   return(Result);
}

/*********************************************************************
*
*   Function:       AudioConfigureEndpoints
*
*   Description:
*
*********************************************************************/

u32 AudioConfigureEndpoints(AUDIO_STREAMING_DEV_DATA *pDevice)
{
  u32 Result = ADI_DEV_RESULT_SUCCESS;
  ADI_ENUM_ENDPOINT_INFO EnumEpInfo;
  static ADI_USB_APP_EP_INFO EpInfo[MAX_ENDPOINTS_OBJS] = {0};
  
  EnumEpInfo.pUsbAppEpInfo = &EpInfo[0];
  EnumEpInfo.dwEpTotalEntries = sizeof(EpInfo)/sizeof(ADI_USB_APP_EP_INFO);
  EnumEpInfo.dwEpProcessedEntries = 0;

  Result = EnumerateEndpoints(&EnumEpInfo, pDevice);

  if (Result != ADI_DEV_RESULT_SUCCESS)
  {
      return ADI_DEV_RESULT_FAILED;
  }

  if(EpInfo[0].eDir == USB_EP_IN)
  {
    pDevice->audio_IN_EP  = EpInfo[0].dwEndpointID;
    pDevice->audio_OUT_EP = EpInfo[1].dwEndpointID;
  }
  else
  {
    pDevice->audio_OUT_EP  =  EpInfo[0].dwEndpointID;
    pDevice->audio_IN_EP   =  EpInfo[1].dwEndpointID;   
  }

  return Result;
}

/*********************************************************************
*
*   Function:       pddControl
*
*   Description:    List of I/O control commands to the driver
*
*********************************************************************/
static u32 adi_pdd_Control(
    ADI_DEV_PDD_HANDLE PDDHandle,   /* PDD handle */
    u32 Command,                    /* command ID */
    void *pArg                      /* pointer to argument */
)
{
    AUDIO_STREAMING_DEV_DATA   *pDevice;
    u32 Result = ADI_DEV_RESULT_SUCCESS;
    pDevice = (AUDIO_STREAMING_DEV_DATA *)&adi_usb_audio_def;
    PDEVICE_DESCRIPTOR pDevDesc;
    signed char iProductString, iManufacturerString;

    switch(Command)
    {
        case (ADI_DEV_CMD_SET_DATAFLOW_METHOD):
        case (ADI_DEV_CMD_SET_DATAFLOW):
        case (ADI_USB_CMD_ENABLE_USB):
        {
            /* Send device specific command peripheral driver */
            Result = adi_dev_Control(pDevice->PeripheralDevHandle,
                                Command,
                                (void*)pArg);
        }                                
        break;

        case (ADI_USB_CMD_CLASS_SET_CONTROLLER_HANDLE):
        {
             pDevice->PeripheralDevHandle =(ADI_DEV_DEVICE_HANDLE)pArg;
        }
        break;

        case ADI_USB_CMD_CLASS_GET_CONTROLLER_HANDLE:
        {
             u32 *pArgument = (u32*)pArg;
               *pArgument = (u32)pDevice->PeripheralDevHandle;
        }
        break;

        case ADI_USB_CMD_CLASS_CONFIGURE:
        {
             Result =  DevAudioConfigure();
        }
        break;

        case ADI_DEV_CMD_GET_PERIPHERAL_DMA_SUPPORT:
        break;

        case ADI_USB_AUDIO_CMD_SET_BUFFER:
        {
          adi_pdd_Read(NULL,
             ADI_DEV_1D,
             (ADI_DEV_BUFFER *)pArg);
        }
        break;
        
        case ADI_USB_AUDIO_CMD_IS_DEVICE_CONFIGURED:
        {
            if(pDevice->deviceReady == TRUE)
            {
#if 1                
                Result = AudioConfigureEndpoints(pDevice);

                if (Result != ADI_DEV_RESULT_SUCCESS)
                {
                    *((u32 *)pArg) = FALSE;
                    break;
                }
#endif
                /*
                Device configured and endpoints set
                now tell the application we are ready
                */
                *((u32 *)pArg) = TRUE;
            }
            else
            {
                *((u32 *)pArg) = FALSE;
            }
        }   
        break;
        
        case ADI_USB_AUDIO_CMD_SET_VID:
        {
            pDevDesc = adi_usb_GetDeviceDescriptor();
            if (!pDevDesc)
            {
                Result = ADI_DEV_RESULT_FAILED;
                break;
            }

            pDevDesc->wIdVendor = (u16)pArg;
        }            
        break;
        
        /* Override default Product ID */
        case ADI_USB_AUDIO_CMD_SET_PID:
        {
            pDevDesc = adi_usb_GetDeviceDescriptor();
            if (!pDevDesc)
            {
                Result = ADI_DEV_RESULT_FAILED;
                break;
            }

            pDevDesc->wIdProduct = (u16)pArg;
        }            
        break;
        
        /* Override default Product string */
        case ADI_USB_AUDIO_CMD_SET_PRODUCT_STRING:
            pDevDesc = adi_usb_GetDeviceDescriptor();
            if (!pDevDesc)
            {
                Result = ADI_DEV_RESULT_FAILED;
                break;
            }

            iProductString = adi_usb_CreateString((char *)pArg);
            if ((iProductString > 0) && (iProductString < USB_MAX_STRINGS))
                pDevDesc->bIProduct = iProductString;
        break;        
        
        /* Override default Manufacturer string */
        case ADI_USB_AUDIO_CMD_SET_MANUFACTURER_STRING:
            pDevDesc = adi_usb_GetDeviceDescriptor();
            if (!pDevDesc)
            {
                Result = ADI_DEV_RESULT_FAILED;
                break;
            }

            iManufacturerString = adi_usb_CreateString((char *)pArg);
            if ((iManufacturerString > 0) && (iManufacturerString < USB_MAX_STRINGS))
                pDevDesc->bIManufacturer = iManufacturerString;
        break;        

        default:
            Result = ADI_DEV_RESULT_NOT_SUPPORTED;
        break;
    }
    return(Result);
}

/*********************************************************************
*
* Function:   EndpointCompleteCallback
*
* Description:  Handle Endpoint data traffic specific to
*         this device class. 
*
*********************************************************************/
void EndpointCompleteCallback(void *Handle, u32  Event, void *pArg)
{
	switch(Event)
    {
        /* Transmit data to Host complete (Device Mode) */
        case ADI_USB_EVENT_DATA_TX:
        break;

        /* Recieve data from Host complete (Device Mode)*/
        case ADI_USB_EVENT_DATA_RX:
        break;

        /* Recieved a packet but we have no buffer */
        case ADI_USB_EVENT_PKT_RCVD_NO_BUFFER:
        break;

        default:
        break;
    }    
}

/*********************************************************************
*
* Function:   EndpointZeroCallback
*
* Description:  Handle Endpoint 0 data traffic specific to
*         this device class. Including the Audio
*         specific requests.
*
*********************************************************************/
void EndpointZeroCallback( void *Handle, u32 Event, void *pBuffer)
{
AUDIO_STREAMING_DEV_DATA   *pDevice;
pDevice = (AUDIO_STREAMING_DEV_DATA *)&adi_usb_audio_def;

u16 * ControlData = NULL;
u16 CodecReg = 0x0000;

  switch(Event)
  {
    /* Host sent Set Configuration Command */
    case ADI_USB_EVENT_SET_CONFIG:
    {
      if(((u32)pBuffer) == 1)
      {
        pDevice->deviceReady = TRUE;
      }
      else
      {
        pDevice->deviceReady = FALSE;
      }
    }
    break;

    /* Host sent Set Interface Command */
    case ADI_USB_EVENT_SET_INTERFACE:
    {
    }
    break;
    
    case ADI_USB_EVENT_RX_COMPLETE:
    {
#if 1        
      if(wValidAudioPkt == true)
      {  
        wValidAudioPkt = false;

        /* Get the EP0 buffer in the correct format */
        ADI_DEV_1D_BUFFER  *pEP0Buffer  = (ADI_DEV_1D_BUFFER *)pBuffer;
        ControlData = (u16 *)pEP0Buffer->Data;
        CodecReg = (ControlData[1] << 8) | ControlData[0];

        if(bMuteControl == true)
        {
            PRINT_BYTE("Mute Control Data", CodecReg);
        }
        else
        {       
            PRINT_SHORT("Volume Control Data", CodecReg);
        }
      }
#endif      
    }
    break;
            
    case ADI_USB_EVENT_SETUP_PKT:
    {
      /* Get the EP0 buffer in the correct format */
      ADI_DEV_1D_BUFFER  *pEP0Buffer  = (ADI_DEV_1D_BUFFER *)pBuffer;
       
      /* Get the buffer data in USB packet format */
      USB_SETUP_PACKET* pSetupData = (USB_SETUP_PACKET*)pEP0Buffer->Data;
 
      /* Determine what type of request we have */
      if ((USB_CONTROL_REQUEST_GET_TYPE (pSetupData->bmRequestType) == USB_CONTROL_REQUEST_TYPE_CLASS) &&
        (USB_CONTROL_REQUEST_GET_RECIPIENT (pSetupData->bmRequestType) == USB_CONTROL_REQUEST_TYPE_INTERFACE) &&
        (LOBYTE (pSetupData->wIndex) == INTERFACE_FOR_AUDIOCONTROL))
      {
#if 1          
          wValidAudioPkt = true;
#endif          
          /* Process Audio GET/SET Requests */
          if (USB_CONTROL_REQUEST_GET_DIRECTION(pSetupData->bmRequestType) == USB_CONTROL_REQUEST_TYPE_OUT)
          {
             /* Audio SET Request */
             AudioSetRequest (pSetupData->bRequest, HIBYTE(pSetupData->wIndex), pSetupData->wValue, pSetupData->wLength, (u8 *)pEP0Buffer->Data);
          }           
          else
          {
            /* Audio GET Request */
            AudioGetRequest (pSetupData->bRequest, HIBYTE(pSetupData->wIndex), pSetupData->wValue, pSetupData->wLength, pSetupData->wLength, (u8 *)pEP0Buffer);
          }
      }
    }
    break;

    case ADI_USB_EVENT_VBUS_TRUE:
        {
          (pDevice->DMCallback) ( Handle, ADI_USB_EVENT_VBUS_TRUE, 0);
        }         
    break;

    case ADI_USB_EVENT_SUSPEND:
        case ADI_USB_EVENT_DISCONNECT:
        {
         (pDevice->DMCallback) ( Handle, ADI_USB_EVENT_DISCONNECT, 0);
        }
    break;

    case ADI_USB_EVENT_ROOT_PORT_RESET: // reset signaling detected
    {
          (pDevice->DMCallback) ( Handle, ADI_USB_EVENT_ROOT_PORT_RESET, 0);
    }         
    break;

    case ADI_USB_EVENT_VBUS_FALSE:    // cable unplugged
    {
          (pDevice->DMCallback) ( Handle, ADI_USB_EVENT_VBUS_FALSE, 0);
    }
    break;

    case ADI_USB_EVENT_RESUME:      // device resumed
    {
          (pDevice->DMCallback) ( Handle, ADI_USB_EVENT_RESUME, 0);
    }
    break;        
    
    default:
    break;
  }
}

/*------------------------------------------------------------
* AudioGetRequest
*
* Parameters:  
*   bRequest - the request.
*   wIndexHi - the high byte of wIndex
*   wValue - as it comes in the setup packet
*   wLength - as it comes in the setup packet.
*   pNumBytesToTransfer - pointer to a location which receives the number of bytes to be transferred.
*   buffer - pointer to a buffer where the result data is to be stored.
*
* Globals Used:
*   gOutMute, gOutLeftVol, gOutRightVol
*
* Description:
*   This handles the Audio Class Get Requests
*
* Returns:
*   true on success, false on failure (to stall the EP).
*
*------------------------------------------------------------*/
bool AudioGetRequest (u8 bRequest, u8 wIndexHi, u16 wValue, u16 wLength, u16 *pNumBytesToTransfer, u8 *buffer)
  {
    bool retVal = false;

    switch (bRequest)
      {
        case GET_CUR:
          switch (wIndexHi)
            {
              case AUDIO_UNITID_VOLUME_OUT:
                switch (HIBYTE(wValue))
                  {
                    case USB_AC_FEATURE_UNIT_MUTE_CONTROL:
                    {         
                      AudioControlData[0] = 0x01;
                         
                      /* Return the Audio control data */
                      ReturnAudioControlData(wLength);
                       
                      retVal = true;                           
                    }           
                    break;
                    
                    case USB_AC_FEATURE_UNIT_VOLUME_CONTROL:
                      switch (LOBYTE(wValue))
                        {
                          case 0:
                          {  
                            AudioControlData[0] = 0x00;
                            AudioControlData[1] = 0xD2;
                            
                            /* Return the Audio control data */
                            ReturnAudioControlData(wLength);
                            
                            retVal = true;
                          }                            
                          break;
                          
                          case 1:
                          break;
                          
                          case 2:
                          break;
                          
                          case 0xff:
                          break;
                          
                          default:
                          break;
                        }
                      break;
                  }
                break;
              default:
              break;
            }
          break;
        case GET_MIN:
          switch (wIndexHi)
            {
              case AUDIO_UNITID_VOLUME_OUT:
              {  
                AudioControlData[0] = 0x00;
                AudioControlData[1] = 0xD2;
              
                /* Return the Audio control data */
                ReturnAudioControlData(wLength);
               
                retVal = true;                
              }  
              break;
              
              default:
              break;
            }
          break;
        case GET_MAX:
          switch (wIndexHi)
            {
              case AUDIO_UNITID_VOLUME_OUT:
              {  
                AudioControlData[0] = 0x00;
                AudioControlData[1] = 0x1C;
              
                /* Return the Audio control data */
                ReturnAudioControlData(wLength);
                             
                retVal = true;                
              }  
              break;
              
              default:
              break;
            }
          break;
        case GET_RES:
          switch (wIndexHi)
            {
              case AUDIO_UNITID_VOLUME_OUT:
              {  
                AudioControlData[0] = 0x00;
                AudioControlData[1] = 0x02;
              
                /* Return the Audio control data */
                ReturnAudioControlData(wLength);
                            
                retVal = true;                
              }                
              break;
              
              default:
              break;
            }
          break;
        default:
        break;
      }

    return(retVal);
}

/*------------------------------------------------------------
* AudioSetRequest
*
* Parameters:  
*   bRequest - the request.
*   wIndexHi - the high byte of wIndex
*   wValue - as it comes in the setup packet
*   wLength - as it comes in the setup packet.
*   buffer - pointer to a buffer where the associated data is stored
*
* Globals Used:
*   gOutMute, gOutLeftVol, gOutRightVol
*
* Description:
*   This handles the Audio Class Set Requests
*
* Returns:
*   true on success, false on failure.
*
*------------------------------------------------------------*/
bool AudioSetRequest (u8 bRequest, u8 wIndexHi, u16 wValue, u16 wLength, u8 *buffer)
  {
    bool retVal = false;

    switch (bRequest)
      {
        case SET_CUR:
          switch (wIndexHi)
            {
              case AUDIO_UNITID_VOLUME_OUT:
              {
                switch (HIBYTE(wValue))
                  {
                    case USB_AC_FEATURE_UNIT_MUTE_CONTROL:
                    {
#if 1                        
                        bMuteControl = true;
                        bVolumeControl = false;
#endif                        
                    }
                    break;
                    
                    case USB_AC_FEATURE_UNIT_VOLUME_CONTROL:
                    {
#if 1                        
                        bMuteControl = false;
                        bVolumeControl = true;
#endif                        
                        
                      switch (LOBYTE(wValue))
                        {
                          case 0:
                          break;
                          
                          case 1:
                          break;
                          
                          case 2:
                          break;
                          
                          case 0xff:
                          break;
                          
                          default:
                          break;
                        }
                    }                       
                    break;
                  }
              } 
              break;
              default:
              break;
            }
          break;
        default:
        break;
      }
    return(retVal);
}

/*------------------------------------------------------------
* ReturnAudioControlData
*------------------------------------------------------------*/
void ReturnAudioControlData (u16 bytesToReturn)
{
    AUDIO_STREAMING_DEV_DATA   *pDevice;
    pDevice = (AUDIO_STREAMING_DEV_DATA *)&adi_usb_audio_def;    
    
    AudioControlBuffer.Data = &AudioControlData;
    AudioControlBuffer.ElementCount = bytesToReturn;
    AudioControlBuffer.ElementWidth = 1;
    AudioControlBuffer.CallbackParameter = &AudioControlBuffer;
    AudioControlBuffer.ProcessedElementCount = 0;
    AudioControlBuffer.ProcessedFlag = 0;
    AudioControlBuffer.pNext = NULL;
    
    AudioControlBuffer.Reserved[BUFFER_RSVD_EP_ADDRESS] = 0x0;

    adi_dev_Write(pDevice->PeripheralDevHandle,
                  ADI_DEV_1D,
                 (ADI_DEV_BUFFER*)&AudioControlBuffer);     
};

/**************************************************************************
 *
 * USB Audio Class driver entry point (Device Mode)
 *
 **************************************************************************/
ADI_DEV_PDD_ENTRY_POINT ADI_USB_Device_AudioClass_Entrypoint = {
    adi_pdd_Open,
    adi_pdd_Close,
    adi_pdd_Read,
    adi_pdd_Write,
    adi_pdd_Control
};

