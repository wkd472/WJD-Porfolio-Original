#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include "mobiapp_cgi.h"
#include <strings.h>

/***********************************
  Defines & Global Declaration
 ************************************/

#define ERROR   1
#define SERVER_PORT1 14000
#define INTERFACE "eth0"
#define NUMPLACEHOLDERS 33
#define NETWORKCFG "/etc/network.conf"

#define IFCONFIG "/sbin/ifconfig eth0 | grep \"inet addr\" | awk -F: '{print $2}' | awk -F\" \"  '{print $1}' > ./ip"
#define _DEBUG
#ifdef _DEBUG
#define DBG(fmt, args...) fprintf(stdout, "Mobicam3_DBG: " fmt, ## args)
#else
#define DBG(fmt, args...)
#endif

int newSd, clntSd, sd;
char cPath[50];

/************************************
  Function Declaration
 *************************************/
void default_thread (void *arg);
void thrdFxn_read_line (void *arg);
void initFTP();
void Applynetwork();
void findBroadCastIP (char * cpIPAddiResults, char * cpSubnetMask,
                      char * cpBroadcastIP);

void
SignalHandler ()
{
   printf ("Closing socket\n");
   close (sd);
}


main (int argc, char *argv[])
{
/*Variable declaration */
   int cliLen, iThread_read_line;
   int resUseFlag = 1;
   pid_t pid;
   char cmd[50] = { 0 };
   char IpAddr[16] = { 0 };
   struct sockaddr_in cliAddr, servAddr;
   pthread_t thread_read_line, default_thread_id;

   if (argc < 2)
   {
      printf ("USAGE : ./web_app WEB_DIRECTORY_PATH \n");
      exit (0);
   }
   /*Get the process ID from the process name */
   pid = GetPIDFromName ("appWeb");
   if (pid != 0)
   {
      sprintf (cmd, "kill -9 %d", pid);
      system (cmd);
      sleep (2);
   }

   /*Get the process ID from the process name */
   pid = GetPIDFromName ("thttpd");
   if (pid != 0)
   {
      sprintf (cmd, "kill -9 %d", pid);
      system (cmd);
      sleep (2);
   }
   UpdateAllhtml (argv[1]);

   strcpy (cPath, argv[1]);

#if 0
   /* Signal handler */
   if (signal (SIGINT, SignalHandler) < 0)
   {
      return -1;
   }
   if (signal (SIGQUIT, SignalHandler) < 0)
   {
      return -1;
   }
#endif

   /*Initialize ftp*/
   initFTP();
   /* Start THTTPD server on board */
   GetIP ("eth0", IpAddr);
   sprintf (cmd, "thttpd -d %s -c \"**\" -h %s -u root", argv[1], IpAddr);
   system (cmd);

   pthread_create (&default_thread_id, NULL, (void *) default_thread, NULL);

   /* create socket */
   sd = socket (AF_INET, SOCK_STREAM, 0);

   /*Check whether the socket is created or not. */
   if (sd < 0)                  /*if the socket is not created */
   {
      perror ("cannot open socket ");
      return ERROR;
   }

   /* bind server port */
   servAddr.sin_family = AF_INET;
   servAddr.sin_addr.s_addr = htonl (INADDR_ANY);
   servAddr.sin_port = htons (SERVER_PORT1);
   if (setsockopt (sd, SOL_SOCKET, SO_REUSEADDR, (char *) &resUseFlag,
                   sizeof (int)) == (-1))
   {
      printf (" %s : Failed to set Reuseaddr \n", __FUNCTION__);
   }

   if (bind (sd, (struct sockaddr *) &servAddr, sizeof (servAddr)) < 0)
   {
      perror ("cannot bind port ");
      close (sd);
      return ERROR;
   }

/*Specify a queue limit for the incomming connection. */
   if (listen (sd, 5) < 0)
   {
      printf ("Error in listen\n");
      close (sd);
      return ERROR;
   }

   while (1)
   {
      /*extract the first connection request on the queue of pending connections. */
      newSd = accept (sd, (struct sockaddr *) &cliAddr, (size_t *) & cliLen);

      /*check whether the connection is accepted or not. */
      if (newSd < 0)
      {
         perror ("CAN NOT ACCEPT CONNECTION ");
         close (sd);
         return 0;
      }
      clntSd = newSd;
      /*Creating thread */
      iThread_read_line =
         pthread_create (&thread_read_line, NULL, (void *) thrdFxn_read_line,
                         NULL);
      if (iThread_read_line == 0)
      {
         printf ("thrdFxn_read_line is created successfully\n");
      }
      else
      {
         printf ("Unable to create thrdFxn_read_line\n");
         close (sd);
         close (clntSd);
         break;
      }

   }
   return 0;
}

void
default_thread (void *arg)
{

   pid_t pid_1, pid_2;
   pthread_t defaultmd;
   char cmd[30];

   pid_1 = GetPIDFromName ("mobiapp");
   if (pid_1 != 0)
   {
      sprintf (cmd, "kill -9 %d", pid_1);
      system (cmd);

   }
   sleep (1);
   pid_2 = GetPIDFromName ("mgapp");
   if (pid_2 != 0)
   {
      sprintf (cmd, "kill -9 %d", pid_2);
      system (cmd);
   }
   sleep (2);

   system ("mobiapp -f mc2_3s.lua");
}


/*******************************************************************************
 * Function: GetPIDFromName
 *
 * Description: Get the process ID from the name.
 *
 * Parameters:
 *  [IN]  unsigned char *processName - String containing the process name
 *
 * Return Value:
 *  pid_t - Returns the PID value on success otherwise 0 (zero)
 ******************************************************************************/
pid_t
GetPIDFromName (const char *processName)
{
   /*Variable declaration */
   DIR *dir;
   struct dirent *next;
   pid_t retVal = 0;
   FILE *status;
   char fileName[20];
   char buffer[20];
   unsigned char entryName[20];

   if (processName == NULL)
      return retVal;

   dir = opendir ("/proc");
   if (!dir)
   {
      printf ("ERROR: Unable to open /proc\n");
      return retVal;
   }
   while ((next = readdir (dir)) != NULL)
   {
      /* Must skip ".." since that is outside /proc */
      if (strcmp (next->d_name, "..") == 0)
         continue;

      /* If it isn't a number, we don't want it */
      if (!isdigit (*next->d_name))
         continue;

      sprintf (fileName, "/proc/%s/status", next->d_name);
      if (!(status = fopen (fileName, "r")))
      {
         continue;
      }
      if (fgets (buffer, 20 - 1, status) == NULL)
      {
         fclose (status);
         continue;
      }
      fclose (status);
      /* Buffer should contain a string like "Name:   binary_name" */
      sscanf (buffer, "%*s %s", entryName);
      if (strcmp ((const char *) entryName, (const char *) processName) == 0)
      {
         retVal = strtol (next->d_name, NULL, 0);
         break;
      }
   }
   return retVal;
}

/*****************************************************************
 * Function : GetIP
 *
 * Description : It will return IP address of n/w device
 *
 * Parameters :
 *    [IN] char *ifname      -    character pointer
 *                            i.e. eth0, wlan0 , usb0 etc
 *    [OUT]char *addr       -     IPaddress buffer pointer
 *
 * RETURNS : int
 *                   RET_OK      - if success
 *                   RET_ERROR   - if error
 *****************************************************************/
int
GetIP (char *ifname, char *ip)
{
   struct ifreq ifr;
   struct sockaddr_in *sin = (struct sockaddr_in *) &ifr.ifr_addr;
   int sock = socket (AF_INET, SOCK_DGRAM, 0);

   //get the interface IP address
   strcpy (ifr.ifr_name, ifname);
   ifr.ifr_addr.sa_family = AF_INET;
   //Get ipaddress using SIOCGIFADDR ioctl
   if (ioctl (sock, SIOCGIFADDR, &ifr) < 0)
   {
      perror ("SIOCGIFADDR");
      if (close (sock) < 0)
      {
         printf ("ERROR: %s: Unable to close sock\n", __FUNCTION__);
      }
      return -1;
   }
   sprintf (ip, "%s", inet_ntoa (sin->sin_addr));

   if (close (sock) < 0)
   {
      printf ("ERROR: %s: Unable to close sock\n", __FUNCTION__);
   }
   return 0;
}

/*******************************************************************
 *  Function : UpdateAllhtml
 *
 * Description : It creates some html files
 *
 * Parameters :
 * [IN] char *path      -    Path where to create html files
 *
 *  RETURNS : int
 *                   0 - success
 ******************************************************************/


int
UpdateAllhtml (char *path)
{
   FILE *FileHandle = NULL;                      /* File Descriptor */
   FILE *Fps; 
   char ipAddr[16] = { 0 };
   char cmd1[150] = { 0 };
   char subNetMask[16] = { 0 };
   char gateway[16] = { 0 };
   int len;
   GetIP ("eth0", ipAddr);
   
   system("ifconfig eth0 | grep \"Mask\" | awk -F: \'{print $4}\' | awk -F \" \" \'{print $1}\' > /tmp/netmask");

   Fps = fopen("/tmp/netmask", "rb");
   if (Fps == NULL)
   {
      perror ("\n\n"); 
   }
 
   fread(subNetMask, 16, 1,Fps);
//   fgets(subNetMask, 16, Fps);
   len = strlen(subNetMask);
	if( subNetMask[len-1] == '\n' )
	    subNetMask[len-1] = 0;
   fclose(Fps);

   system("route | grep \"default\" | awk -F \" \" \'{print $2}\' > /tmp/gateway");
   Fps = fopen("/tmp/gateway", "rb");
   if (Fps == NULL)
   {
      perror ("\n\n"); 
   }

   fread(gateway, 16, 1,Fps);
   len = 0;
   len = strlen(gateway);
	if( gateway[len-1] == '\n' )
	    gateway[len-1] = 0;
   fclose(Fps);

   printf ("Updates\n\n\n\n\n");
   sprintf (cmd1, "%s/Indexrefresh.html", path);	

   FileHandle = fopen (cmd1, "w");

   if (FileHandle != NULL)
   {

      fprintf (FileHandle, "<html>\n");
      fprintf (FileHandle, "<body>\n");
      fprintf (FileHandle,
               "<META HTTP-EQUIV=\"refresh\" CONTENT=\"0;URL=http://%s/operator/livevideo.html\">\n",
               ipAddr);
      fprintf (FileHandle, "</body>\n");
      fprintf (FileHandle, "</html>\n");
      fclose (FileHandle);
      FileHandle = NULL;
   }
   else
   {
      printf ("FileHandle1 is null \n ");
   }


   memset (cmd1, 0x0, strlen (cmd1));
   sprintf (cmd1, "%s/Restartrefresh.html", path);

   FileHandle = fopen (cmd1, "w");

   if (FileHandle != NULL)
   {

      fprintf (FileHandle, "<html>\n");
      fprintf (FileHandle, "<body>\n");
      fprintf (FileHandle,
               "<META HTTP-EQUIV=\"refresh\" CONTENT=\"0;URL=http://%s/operator/restart.html\">\n",
               ipAddr);
      fprintf (FileHandle, "</body>\n");
      fprintf (FileHandle, "</html>\n");
      fclose (FileHandle);
      FileHandle = NULL;
   }
   else
   {
      printf ("FileHandle2 is null \n ");
   }

   memset (cmd1, 0x00, 150);

   sprintf (cmd1, "%s/Networkrefresh.html", path);

   FileHandle = fopen (cmd1, "w");

   if (FileHandle != NULL)
   {

      fprintf (FileHandle, "<html>\n");
      fprintf (FileHandle, "<body>\n");
      fprintf (FileHandle,
               "<META HTTP-EQUIV=\"refresh\" CONTENT=\"0;URL=http://%s/operator/network.html\">\n",
               ipAddr);
      fprintf (FileHandle, "</body>\n");
      fprintf (FileHandle, "</html>\n");
      fclose (FileHandle);
      FileHandle = NULL;
   }
   else
   {
      printf ("FileHandle3 is null \n ");
   }

   memset (cmd1, 0x00, 150);
   sprintf (cmd1, "%s/Systemrefresh.html", path);

   FileHandle = fopen (cmd1, "w");

   if (FileHandle != NULL)
   {

      fprintf (FileHandle, "<html>\n");
      fprintf (FileHandle, "<body>\n");
      fprintf (FileHandle,
               "<META HTTP-EQUIV=\"refresh\" CONTENT=\"0;URL=http://%s/operator/system.html\">\n",
               ipAddr);
      fprintf (FileHandle, "</body>\n");
      fprintf (FileHandle, "</html>\n");
      fclose (FileHandle);
   }
   else
   {
      printf ("FileHandle4 is null \n ");
   }

   memset (cmd1, 0x00, 150);
   sprintf (cmd1, "%s/Motionrefresh.html", path);

   FileHandle = fopen (cmd1, "w");

   if (FileHandle != NULL)
   {

      fprintf (FileHandle, "<html>\n");
      fprintf (FileHandle, "<body>\n");
      fprintf (FileHandle,
               "<META HTTP-EQUIV=\"refresh\" CONTENT=\"0;URL=http://%s/operator/motion_alarm.html\">\n",
               ipAddr);
      fprintf (FileHandle, "</body>\n");
      fprintf (FileHandle, "</html>\n");
      fclose (FileHandle);
   }
   else
   {
      printf ("FileHandle4 is null \n ");
   }

   memset (cmd1, 0x00, 150);
   sprintf (cmd1, "%s/admin/Userrefresh.html", path);

   FileHandle = fopen (cmd1, "w");

   if (FileHandle != NULL)
   {

      fprintf (FileHandle, "<html>\n");
      fprintf (FileHandle, "<body>\n");
      fprintf (FileHandle,
               "<META HTTP-EQUIV=\"refresh\" CONTENT=\"0;URL=http://%s/admin/Usermgmt.html\">\n",
               ipAddr);
      fprintf (FileHandle, "</body>\n");
      fprintf (FileHandle, "</html>\n");
      fclose (FileHandle);
   }
   else
   {
      printf ("FileHandle4 is null \n ");
   }

   memset (cmd1, 0x00, 150);
   sprintf (cmd1, "%s/Schedulerefresh.html", path);

   FileHandle = fopen (cmd1, "w");

   if (FileHandle != NULL)
   {

      fprintf (FileHandle, "<html>\n");
      fprintf (FileHandle, "<body>\n");
      fprintf (FileHandle,
               "<META HTTP-EQUIV=\"refresh\" CONTENT=\"0;URL=http://%s/operator/schedule.html\">\n",
               ipAddr);
      fprintf (FileHandle, "</body>\n");
      fprintf (FileHandle, "</html>\n");
      fclose (FileHandle);
   }
   else
   {
      printf ("FileHandle4 is null \n ");
   }

   memset (cmd1, 0x00, 150);
   sprintf (cmd1, "%s/Smtprefresh.html", path);

   FileHandle = fopen (cmd1, "w");

   if (FileHandle != NULL)
   {

      fprintf (FileHandle, "<html>\n");
      fprintf (FileHandle, "<body>\n");
      fprintf (FileHandle,
               "<META HTTP-EQUIV=\"refresh\" CONTENT=\"0;URL=http://%s/operator/serversetting.html\">\n",
               ipAddr);
      fprintf (FileHandle, "</body>\n");
      fprintf (FileHandle, "</html>\n");
      fclose (FileHandle);
   }
   else
   {
      printf ("FileHandle4 is null \n ");
   }

   memset (cmd1, 0x00, 150);
   sprintf (cmd1, "%s/Ftprefresh.html", path);

   FileHandle = fopen (cmd1, "w");

   if (FileHandle != NULL)
   {

      fprintf (FileHandle, "<html>\n");
      fprintf (FileHandle, "<body>\n");
      fprintf (FileHandle,
               "<META HTTP-EQUIV=\"refresh\" CONTENT=\"0;URL=http://%s/operator/serversetting.html\">\n",
               ipAddr);
      fprintf (FileHandle, "</body>\n");
      fprintf (FileHandle, "</html>\n");
      fclose (FileHandle);
   }
   else
   {
      printf ("FileHandle4 is null \n ");
   }

   memset (cmd1, 0x00, 150);

   sprintf (cmd1, "%s/js/sldrchnlip.js", path);

   FileHandle = fopen (cmd1, "w");

   if (FileHandle != NULL)
   {

      fprintf (FileHandle, "function setchnlip()\n");
      fprintf (FileHandle, "{\n");
      fprintf (FileHandle, "\tdocument.getElementById(\"chnl\").value= 0;\n");
      fprintf (FileHandle, "}\n");
      fclose (FileHandle);
      FileHandle = NULL;
   }
   else
   {
      printf ("FileHandle3 is null \n ");
   }

   memset (cmd1, 0x00, 150);

   sprintf (cmd1, "%s/js/home.js", path);

   FileHandle = fopen (cmd1, "w");

   if (FileHandle != NULL)
   {

      fprintf (FileHandle, "function setvalues()\n");
      fprintf (FileHandle, "{\n");
      fprintf (FileHandle,
               "\tdocument.forms[0].rtsp_url1.value = \"rtsp://%s/test\"\n",
               ipAddr);
      fprintf (FileHandle,
               "\tdocument.forms[0].rtsp_url2.value = \"rtsp://%s/test1\"\n",
               ipAddr);
      fprintf (FileHandle, "}\n");
      fclose (FileHandle);
      FileHandle = NULL;
   }
   else
   {
      printf ("FileHandle3 is null \n ");
   }

   memset (cmd1, 0x00, 150);

   sprintf (cmd1, "%s/js/network.js", path);

   FileHandle = fopen (cmd1, "w");

   if (FileHandle != NULL)
   {

      fprintf (FileHandle, "function setvalues()\n");
      fprintf (FileHandle, "{\n");
      fprintf (FileHandle,
               "\tdocument.forms[0].cameraip.value = \"%s\";\n",
               ipAddr);
      fprintf (FileHandle,
               "\tdocument.forms[0].subnetmask.value = \"%s\";\n",
               subNetMask);
      fprintf (FileHandle,
               "\tdocument.forms[0].gateway.value = \"%s\";\n",
               gateway);
      fprintf (FileHandle, "}\n");
      fclose (FileHandle);
      FileHandle = NULL;
   }
   else
   {
      printf ("FileHandle3 is null \n ");
   }


   memset (cmd1, 0x00, 150);

   sprintf (cmd1, "%s/js/video.js", path);

   FileHandle = fopen (cmd1, "w");

   if (FileHandle != NULL)
   {

      fprintf (FileHandle, "function setvalues(url_ip)\n");
      fprintf (FileHandle, "{\n");
      fprintf (FileHandle,
               "\tdocument.forms[0].RtpValue.value = \"rtp_tcp\";\n");
      fprintf (FileHandle, "\tdocument.forms[0].rtp_val.value = 1;\n");
      fprintf (FileHandle,
               "\tdocument.forms[0].rtsp_url.value = \"rtsp://\"+url_ip+\"/test\";\n");

      //fprintf(FileHandle, "\talert(document.forms[0].rtsp_url.value);\n");
      fprintf (FileHandle, "}\n");
      fclose (FileHandle);
      FileHandle = NULL;
   }
   else
   {
      printf ("FileHandle3 is null \n ");
   }


   return 0;
}

/*******************************************************************************
 * Function: thrdFxn_read_line
 *
 * Description: This function will receive the data from client and according
 the data received make new dvr.lua and run it using system call
 *
 * Parameters:
 *    NONE
 *
 * Return Value:
 *       NONE
 ******************************************************************************/
void
thrdFxn_read_line (void *arg)
{
   /*Variable declaration */
   char *pval = NULL, *pval1 = NULL;
   int changeDone = 0, changeDoneAna = 0, iCnt = 0;
   int iDate = 0, iYear = 0, iMonth = 0, iHour = 0, iMin = 0, iSec = 0;
   int n;
   PACKET *pRecvPacket;
   pid_t pid_1, pid_2;
   INDEX_PAGE *pindex;
   SETTING_PAGE *prcvconfig;
   NETWORK_PAGE *pnetwork;
   SYSTEM_PAGE *psystem;
   ANALYTICS_PAGE *panalytics;
   PAGE_MOT_DET *pMotDet;
   USERMGMT_PAGE *pUserMgmt;
   MOT_DET_INFO *pListInfo;
   CHANNEL channelId;
   int clsockfd;
   struct sockaddr_in serv_addr;
   int sinsize = sizeof (serv_addr);
   char Bcastip[16] = { 0 };

   char ipAddr[16] = { 0 };
   char cmd[100] = { 0 };
   char str[200] = { 0 };
   static int intX_d1 = 2, intY_d1 = 0;
   static int intX_cif = 2, intY_cif = 0;
   pRecvPacket = (PACKET *) calloc (sizeof (PACKET), sizeof (char));
   if (pRecvPacket == NULL)
   {
      printf ("thrdFxn_read_line : malloc failed\n");
      return;
   }
   n = recv (clntSd, (char *) pRecvPacket, sizeof (PACKET), 0);

   /*check whether the data is received or not. */
   if (n < 0)
   {
      perror (" CAN NOT RECEIVE DATA ");
      exit (1);
   }
   else if (n == 0)
   {
      printf (" Size of received data is zero.\n");
      close (clntSd);
      free (pRecvPacket);
      return;
   }
   else
   {
      printf ("pageName VALUE is %d\n", pRecvPacket->pageName);
      switch (pRecvPacket->pageName)
      {
         case NETWORK_PAGE1:
            pnetwork = (NETWORK_PAGE *) pRecvPacket->data;
            printf ("New IP is : %s\n", pnetwork->cameraIp);

            if (pnetwork->changeIP == 1)
            {
               if (pnetwork->changeIPAutomatic == 0)
               {

                  close (clntSd);
			
		  findBroadCastIP(pnetwork->cameraIp, pnetwork->subnetMask, Bcastip); 
                  sprintf (str, "setIP static %s %s %s\n", pnetwork->cameraIp, pnetwork->subnetMask, Bcastip);
                  /* fire the system call to assign static IP configuration  */
                  system (str);
                  /*Get the process ID from the process name */
                  pid_1 = GetPIDFromName ("mobiapp");
                  if (pid_1 != 0)
                  {
                     sprintf (cmd, "kill -9 %d", pid_1);
                     system (cmd);

                  }
                  sleep (1);
                  pid_2 = GetPIDFromName ("mgapp");
                  if (pid_2 != 0)
                  {
                     sprintf (cmd, "kill -9 %d", pid_2);
                     system (cmd);
                  }
                  sleep (2);

               }
               else
               {

                  close (clntSd);
                  close (sd);
                  sprintf (str, "setIP dhcp");
                  /* fire the system call to assign dhcp IP configuration  */
                  system (str);
#if 0
                  system ("/etc/init.d/50network stop");
                  sleep (2);
                  system ("/etc/init.d/50network start");
		 sleep(2);
#endif
                  pid_1 = GetPIDFromName ("mobiapp");
                  if (pid_1 != 0)
                  {
                     sprintf (cmd, "kill -9 %d", pid_1);
                     system (cmd);

                  }
                  sleep (1);
                  pid_2 = GetPIDFromName ("mgapp");
                  if (pid_2 != 0)
                  {
                     sprintf (cmd, "kill -9 %d", pid_2);
                     system (cmd);
                  }
                  sleep (2);
               }
               exit (0);
            }

            break;

         case SYSTEM_PAGE1:

            psystem = (SYSTEM_PAGE *) pRecvPacket->data;
	    printf("REBOOT = %d\n",psystem->rebootSys);
            if (psystem->rebootSys == 1)
            {
		printf("Here in Reboot\n\n\n");
               sleep (2);
               system ("reboot -f");
            }
            else if (psystem->bTimeDate == 1)
            {
               int tmp = 0;
               memset (str, 0x00, 200);
               printf ("TZ= .%s.\n", psystem->chTimeZone);
               unsetenv ("TZ");
               sleep (1);
               tmp = setenv ("TZ", psystem->chTimeZone, 1);
               printf ("TEMP = %d\n", tmp);
               sleep (1);
               system (str);
               if (psystem->bTimeDateNetwork == 1)
               {
                  memset (str, 0x00, 200);
                  sprintf (str, "ntpdate %s", psystem->chIpaddress);
                  system (str);
               }
               else
               {
                  pval = strtok (psystem->chDate, "-");
                  iMonth = atoi (pval);
                  printf ("date : %s\n", psystem->chDate);

                  pval = strtok (NULL, "-");
                  iDate = atoi (pval);


                  pval = strtok (NULL, "\0");
                  iYear = atoi (pval);

                  printf ("time: %s\n", psystem->chTime);
                  pval1 = strtok (psystem->chTime, ":");
                  iHour = atoi (pval1);

                  pval1 = strtok (NULL, ":");
                  iMin = atoi (pval1);
                  pval1 = strtok (NULL, "\0");
                  iSec = atoi (pval1);

                  memset (str, 0x00, 200);

                  sprintf (str, "date %02d%02d%02d%02d%04d", iMonth,
                           iDate, iHour, iMin, iYear);
                  printf ("STR: %s\n", str);
                  system (str);

               }

               system ("hwclock -w");
               sleep (2);
               system ("hwclock -s");
		
            }
            close (clsockfd);
            close (clntSd);
            break;

	 case USER_MGMT_PAGE:
		pUserMgmt = (USERMGMT_PAGE *) pRecvPacket->data;	   
                  memset (str, 0x00, 200);
		char cmd[70] = { 0 };

		if(pUserMgmt->action==ADD_USER || pUserMgmt->action==EDIT_USER )
		{
			sprintf(str, "htpasswd -b WebPages/.htpasswd %s %s", pUserMgmt->userName, pUserMgmt->passWord);
			system(str);
			sleep(1);
                  	memset (str, 0x00, 200);
			if (pUserMgmt->accessLevel == ADMINISTROTR || pUserMgmt->accessLevel == OPERATOR)
			{
				sprintf(str, "htpasswd -b WebPages/operator/.htpasswd %s %s", pUserMgmt->userName, pUserMgmt->passWord);
				system(str);
				sleep(1);
                	  	memset (str, 0x00, 200);
			}
			if (pUserMgmt->accessLevel == ADMINISTROTR)
			{
				sprintf(str, "htpasswd -b WebPages/admin/.htpasswd %s %s", pUserMgmt->userName, pUserMgmt->passWord);
				system(str);
				sleep(1);
			
				if(pUserMgmt->action==ADD_USER)
				{
					sprintf(cmd,"pure-pw useradd %s -u ftpuser -d /temp/ftproot/ -p %s", pUserMgmt->userName, pUserMgmt->passWord);
					system(cmd);
					sleep(1);	
					sprintf(cmd,"pure-pw mkdb");
					system(cmd);
					sleep(1);	
				}			
				else
				{
					sprintf(cmd,"pure-pw usermod %s -p %s", pUserMgmt->userName, pUserMgmt->passWord);
					system(cmd);
					sleep(1);	
					sprintf(cmd,"pure-pw mkdb");
					system(cmd);
					sleep(1);	
				}
			}
			else if(pUserMgmt->accessLevel == OPERATOR)
			{
				if(pUserMgmt->action==ADD_USER)
				{
					sprintf(cmd,"pure-pw useradd %s -u ftpguestuser -d /temp/ftproot/ -p %s", pUserMgmt->userName, pUserMgmt->passWord);
					system(cmd);
					sleep(1);	
					sprintf(cmd,"pure-pw mkdb");
					system(cmd);
					sleep(1);	
				}			
				else
				{
					sprintf(cmd,"pure-pw usermod %s -p %s", pUserMgmt->userName, pUserMgmt->passWord);
					system(cmd);
					sleep(1);	
					sprintf(cmd,"pure-pw mkdb");
					system(cmd);
					sleep(1);	
				}
			}
		}

		else if(pUserMgmt->action==DELETE_USER )
		{
			sprintf(str, "htpasswd -D WebPages/.htpasswd %s", pUserMgmt->userName);
			system(str);
			sleep(1);
                  	memset (str, 0x00, 200);
			if (pUserMgmt->accessLevel == ADMINISTROTR || pUserMgmt->accessLevel == OPERATOR)
			{
				sprintf(str, "htpasswd -D WebPages/operator/.htpasswd %s", pUserMgmt->userName); 
				system(str);
				sleep(1);
                	  	memset (str, 0x00, 200);
			}
			if (pUserMgmt->accessLevel == ADMINISTROTR)
			{
				sprintf(str, "htpasswd -D WebPages/admin/.htpasswd %s", pUserMgmt->userName); 
				system(str);
				sleep(1);
			}

			sprintf(cmd,"pure-pw userdel %s", pUserMgmt->userName);
			system(cmd);
			sleep(1);	
			sprintf(cmd,"pure-pw mkdb");
			system(cmd);
			sleep(1);	
		}

   	    break;
   	    
         default:
            printf ("Error \n");
      }
   }

   pthread_exit (NULL);
}
void initFTP()
{
   pid_t pid;
   char cmd[50] = { 0 };
   int rc=0;
   pid = GetPIDFromName ("betaftpd");
   if (pid != 0)
   {
      sprintf (cmd, "kill -9 %d", pid);
      system (cmd);
      sleep (1);
   }
   pid = GetPIDFromName ("pure-ftpd");
   if (pid != 0)
   {
      sprintf (cmd, "kill -9 %d", pid);
      system (cmd);
      sleep (1);
   }
   system("killall -9 pure-ftpd");

   system("addgroup ftpgroup");
   sleep(1);
   system("adduser -G ftpgroup -h /dev/null -s /etc -D  ftpuser");
   sleep(1);
   system("addgroup ftpguest");
   sleep(1);
   system("adduser -G ftpguest -h /dev/null -s /etc -D ftpguestuser");
   sleep(1);
/*
   system("mkdir -p /temp/ftproot/CameraStorage");
   sleep(1);
   system("mkdir /temp/ftproot/SDCard");
   sleep(1);
   system("mkdir /temp/ftproot/USBMiniHD");
   sleep(1);
*/
   system("umount /temp/ftproot/CameraStorage");
   system("mount -o nolock /data/storage/ /temp/ftproot/CameraStorage");
   sleep(1);
	
   system("umount /temp/ftproot/SDCard");
   system("mount -o nolcok /media/mmcblk0p1/  /temp/ftproot/SDCard");
   sleep(1);

   system("umount /temp/ftproot/USBMiniHD");
   system("mount -o nolcok /media/sda1/ /temp/ftproot/USBMiniHD");
   sleep(1);

   system("chmod -R 755 /temp/ftproot/");
   sleep(1);
   system("chown -R root:ftpgroup /temp/ftproot/");
   sleep(1);


   system("chmod -R 1775 /temp/ftproot/CameraStorage");
   sleep(1);
   system("chmod -R 1775 /temp/ftproot/SDCard");
   sleep(1);
   system("chmod -R 1775 /temp/ftproot/USBMiniHD");
   sleep(1);
   system("chown -R root:ftpgroup /media/");
   sleep(1);
   system("chown -R root:ftpgroup /data/storage");
   sleep(1);

   system("pure-pw useradd admin -u ftpuser -d /temp/ftproot/ -p admin");
   /*start the ftp server*/

   system("pure-pw mkdb");
   sleep(1);
   system("pure-ftpd -j -lpuredb:/etc/pureftpd.pdb &");
   sleep(1);
   


}

/*******************************************************************************
*
* Function Name : findBroadCastIP
*
* Description           : This Function finds BroadCast IP from IP address
*
* Parameters      : [In] CHAR8 *cpIPAddiResults : IP address
*                                                 CHAR8 *cpSubnetMask : Subnest Mask
*                                               [out] CHAR8 *cpBroadcastIP : BroadCast IP
* Return Value          : INT32
*
******************************************************************************/
void
findBroadCastIP (char * cpIPAddiResults, char * cpSubnetMask,
                 char * cpBroadcastIP)
{

   char cIP[100];
   char cSM[100];
   char cBIP[100];
   int index = 0;
   int index1 = 0;
   int index2 = 0;
   int index3 = 0;
   int iResult = 0;

   strcpy (cpBroadcastIP, "");

   while (1)
   {
      /*Check for End of Subnet mask */
      if ((cpSubnetMask[index1] == '.') || (cpSubnetMask[index1] == '\0'))
      {
         while ((cpIPAddiResults[index2] != '.')
                && (cpIPAddiResults[index2] != '\0'))
         {
            cIP[index3] = cpIPAddiResults[index2];
            index3++;
            index2++;
         }

         /* Terminate String */
         cIP[index3] = '\0';
         cSM[index] = '\0';

         /* Find updated value */
         iResult = (atoi (cIP) & atoi (cSM));
         if (iResult == 0)
            iResult = 255;

         sprintf (cBIP, "%d", iResult);
         strcat (cpBroadcastIP, cBIP);

         /* update Variables */
         index = 0;
         index3 = 0;
         index2++;

         /* Exit */
         if (cpSubnetMask[index1] == '\0')
            break;
         else
            strcat (cpBroadcastIP, ".");
      }
      else
      {
         cSM[index] = cpSubnetMask[index1];
         index++;
      }

      index1++;
   }
}
