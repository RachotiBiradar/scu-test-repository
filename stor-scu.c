/*************************************************************************
 *
 *       System: Merge DICOM Toolkit
 *
 *       Author: Merge Healthcare
 *
 *  Description: This is a sample Service Class User application
 *               for the Storage Service Class and the Storage Commitment
 *               service class.  The application has a number of features:
 *               - It can read in images in both the DICOM Part 10 format
 *                 and the DICOM "stream" format.
 *               - The application determines the format of the object
 *                 before reading in.
 *               - The application supports DICOM "Asychronous Window
 *                 negotiation" and will transfer asychronously if
 *                 negotiated.
 *               - The AE Title, host name, and port number of the
 *                 system being connected to can be specified on the
 *                 command line.
 *               - A verbose mode can be specified where detailed
 *                 information is displayed as the application functions.
 *               - The local AE title can be specified on the command
 *                 line.
 *               - The service list (found in the mergecom.app
 *                 configuration file) used by the application to
 *                 determine what services are negotiated can be specified
 *                 on the command line.
 *               - The application will support DICOM Part 10 formated
 *                 compressed/encapsulated if specified on the command
 *                 line.  One note, however, the standard service lists
 *                 found in the mergecom.app file must be extended with
 *                 a transfer syntax list to support these transfer
 *                 syntaxes.
 *               - If specified on the command line, the application will
 *                 send a storage commitment request to the same SCP as
 *                 it is sending images.  The storage commitment request
 *                 will be for the images included on the command line.
 *
 *************************************************************************
 *
 *                      (c) 2012 Merge Healthcare
 *            900 Walnut Ridge Drive, Hartland, WI 53029
 *
 *                      -- ALL RIGHTS RESERVED --
 *
 *  This software is furnished under license and may be used and copied
 *  only in accordance with the terms of such license and with the
 *  inclusion of the above copyright notice.  This software or any other
 *  copies thereof may not be provided or otherwise made available to any
 *  other person.  No title to and ownership of the software is hereby
 *  transferred.
 *
 ************************************************************************/

 /*
  * Standard OS Includes
  */
#include "stor_scu.h"
/****************************************************************************
 *
 *  Function    :   Main
 *
 *  Description :   Main routine for DICOM Storage Service Class SCU
 *
 ****************************************************************************/
int  main(int argc, char** argv);
int main(int argc, char** argv)
{
	SAMP_BOOLEAN            sampBool;
	STORAGE_OPTIONS         options;
	MC_STATUS               mcStatus;
	int                     applicationID = -1, associationID = -1, imageCurrent = 0;
	int                     imagesSent = 0L, totalImages = 0L, fstatus = 0;
	double                  seconds = 0.0;
	void* startTime = NULL, * imageStartTime = NULL;
	char                    fname[512] = { 0 };  /* Extra long, just in case */
	size_t                  totalBytesRead = 0L;
	InstanceNode* instanceList = NULL, * node = NULL;
	FILE* fp = NULL;

	/*
	 * Test the command line parameters, and populate the options
	 * structure with these parameters
	 */

	sampBool = TestCmdLine(argc, argv, &options);

	CheckTestCMD(sampBool);

	/* ------------------------------------------------------- */
	/* This call MUST be the first call made to the library!!! */
	/* ------------------------------------------------------- */
	mcStatus = MC_Library_Initialization(NULL, NULL, NULL);
	CheckForLibInit(mcStatus);

	/*
	 *  Register this DICOM application
	 */
	mcStatus = MC_Register_Application(&applicationID, options.LocalAE);
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		printf("Unable to register \"%s\":\n", options.LocalAE);
		printf("\t%s\n", MC_Error_Message(mcStatus));
		fflush(stdout);
		return(EXIT_FAILURE);
	}

	/*
	 * Create a linked list of all files to be transferred.
	 * Retrieve the list from a specified file on the command line,
	 * or generate the list from the start & stop numbers on the
	 * command line
	 */

	 /* Traverse through the possible names and add them to the list based on the start/stop count */
	traverseimages(&options, &instanceList, fname);

	totalImages = GetNumNodes(instanceList);

	startTime = GetIntervalStart();


	/*
	 *   Open association and override hostname & port parameters if they were supplied on the command line.
	 */
	mcStatus = MC_Open_Association(applicationID,
		&associationID,
		options.RemoteAE,
		&options.RemotePort,
		options.RemoteHostname,
		options.ServiceList);

	/*mcStatus = MC_Open_Association(applicationID,
		&associationID,
		options.RemoteAE,
		options.RemotePort != -1 ? &options.RemotePort : NULL,
		options.RemoteHostname[0] ? options.RemoteHostname : NULL,
		options.ServiceList[0] ? options.ServiceList : NULL);*/
	checkopenassociation(&options, mcStatus);



	mcStatus = MC_Get_Association_Info(associationID, &options.asscInfo);
	checkforassociation(mcStatus);



	//	/*
	//	 * Go through all of the negotiated services.  If encapsulated /
	//	 * compressed transfer syntaxes are supported, this code should be
	//	 * expanded to save the services & transfer syntaxes that are
	//	 * negotiated so that they can be matched with the transfer
	//	 * syntaxes of the images being sent.
	//	 */

	printf("Connected to remote system [%s]\n\n", options.RemoteAE);

	/*
	 * Check User Identity Negotiation and for response
	 */
	checkresponse(&options);

	fflush(stdout);

	/*
	 *   Send all requested images.  Traverse through instanceList to
	 *   get all files to send
	 */
	node = instanceList;
	traverseListandsend(&options, &node, fname, applicationID, associationID, totalImages, &totalBytesRead);



	/*
	 * A failure on close has no real recovery.  Abort the association
	 * and continue on.
	 */
	mcStatus = MC_Close_Association(&associationID);
	checkcloseassociation(mcStatus, associationID);

	/*
	 * Calculate the transfer rate.  Note, for a real performance
	 * numbers, a function other than time() to measure elapsed
	 * time should be used.
	 */

	seconds = GetIntervalElapsed(startTime);

	printf("Data Transferred: %luMB\n", (unsigned long)(totalBytesRead / (1024 * 1024)));
	printf("    Time Elapsed: %.3fs\n", seconds);
	printf("   Transfer Rate: %.1fKB/s\n", ((float)totalBytesRead / seconds) / 1024.0);
	fflush(stdout);
	/*
	 * Release the dICOM Application
	 */
	mcStatus = MC_Release_Application(&applicationID);
	checkreleaseApp(mcStatus);

	/*
	 * Free the node list's allocated memory
	 */
	FreeList(&instanceList);

	/*
	 * Release all memory used by the Merge DICOM Toolkit.
	 */
	checkmemoryrelease();

	fflush(stdout);

	return(EXIT_SUCCESS);
}
void CheckForLibInit(MC_STATUS mcStatus) {
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		PrintError("Unable to initialize library", mcStatus);
		exit(0);
	}
}
void CheckTestCMD(SAMP_BOOLEAN sampBool)
{
	if (sampBool == SAMP_FALSE)
	{
		exit(0);
	}

}
void checkopenassociation(STORAGE_OPTIONS* options, MC_STATUS mcStatus)
{
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		printf("Unable to open association with \"%s\":\n", options->RemoteAE);
		printf("\t%s\n", MC_Error_Message(mcStatus));
		fflush(stdout);
		exit(0);
	}
}

void checkforassociation(MC_STATUS mcStatus) {
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		PrintError("MC_Get_Association_Info failed", mcStatus);
	}
}


void checkmemoryrelease()
{
	if (MC_Library_Release() != MC_NORMAL_COMPLETION)
		printf("Error releasing the library.\n");
}

void checkcloseassociation(MC_STATUS mcStatus, int associationID)
{
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		PrintError("Close association failed", mcStatus);
		MC_Abort_Association(&associationID);
	}
}



void checkreleaseApp(MC_STATUS mcStatus)
{
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		PrintError("MC_Release_Application failed", mcStatus);
	}
}


void traverseListandsend(STORAGE_OPTIONS* options, InstanceNode** node, char* fname, int applicationID, int associationID,int totalImages,size_t* totalBytesRead)
{
	void *imageStartTime = NULL;
	int imagesSent=0;
	double seconds = 0.0;
	SAMP_BOOLEAN sampBool;
	MC_STATUS        mcStatus;
	while (*node)
	{

		imageStartTime = GetIntervalStart();
		/*
		* Determine the image format and read the image in.  If the
		* image is in the part 10 format, convert it into a message.
		*/
		sampBool = ReadImage(options, applicationID, (*node));
		checkReadImage(sampBool,*node);
		totalBytesRead += (*node)->imageBytes;

		/*
		 * Send image read in with ReadImage.
		 *
		 * Because SendImage may not have actually sent an image even though it has returned success,
		 * the calculation of performance data below may not be correct.
		 */
		sampBool = SendImage(options, associationID, (*node));
		checkSendImage(sampBool, *node, associationID, applicationID);
		/*
		 * Save image transfer information in list
		 */
		sampBool = UpdateNode((*node));
		checkImageUpdate(sampBool, *node, associationID, applicationID);


		//CONTINUE FROM HERE
		incrementImageSent(*node, &imagesSent);
		

		mcStatus = MC_Free_Message(&(*node)->msgID);
		if (mcStatus != MC_NORMAL_COMPLETION)
		{
			PrintError("MC_Free_Message failed for request message", mcStatus);
		}

		seconds = GetIntervalElapsed(imageStartTime);
		printf("\tSent %s image (%d of %d), elapsed time: %.3f seconds\n", (*node)->serviceName, imagesSent, totalImages, seconds);

		/*
		 * Traverse through file list
		 */
		*node = (*node)->Next;

	}   /* END for loop for each image */
}


void checkReadImage(SAMP_BOOLEAN sampBool, InstanceNode* node) {
			if (!sampBool)
			{
				(node)->imageSent = SAMP_FALSE;
				printf("Can not open image file [%s]\n", (node)->fname);
				node = (node)->Next;
			}
		}


void checkSendImage(SAMP_BOOLEAN sampBool, InstanceNode* node, int associationID, int applicationID) {
	if (!sampBool)
	{
		(node)->imageSent = SAMP_FALSE;
		printf("Failure in sending file [%s]\n", (node)->fname);
		MC_Abort_Association(&associationID);
		MC_Release_Application(&applicationID);
		
	}
}

void checkImageUpdate(SAMP_BOOLEAN sampBool, InstanceNode* node, int associationID, int applicationID)
{
	if (!sampBool)
	{
		printf("Warning, unable to update node with information [%s]\n", (node)->fname);

		MC_Abort_Association(&associationID);
		MC_Release_Application(&applicationID);

	}
}

void traverseimages(STORAGE_OPTIONS* A_options, InstanceNode** instanceList, char fname[])
{
	SAMP_BOOLEAN sampBool;

	int imageCurrent = 0;
	for (imageCurrent = A_options->StartImage; imageCurrent <= A_options->StopImage; imageCurrent++)
	{
		sprintf(fname, "%d.img", imageCurrent);
		sampBool = AddFileToList(&(*instanceList), fname);
		if (!sampBool)
		{
			printf("Warning, cannot add SOP instance to File List, image will not be sent [%s]\n", fname);
		}
	}
}
void incrementImageSent(InstanceNode* node, int* imagesSent) {
	if ((node)->imageSent == SAMP_TRUE)
	{
		(*imagesSent)++;
	}
	else
	{
		(node)->responseReceived = SAMP_TRUE;
		(node)->failedResponse = SAMP_TRUE;
	}
}
void checkresponse(STORAGE_OPTIONS* options) {
	if (options->ResponseRequested)
	{
		if (!options->asscInfo.PositiveResponseReceived)
		{
			printf("WARNING: Positive response for User Identity requested from\n\tserver, but not received.\n\n");
		}
	}
}
/********************************************************************
 *
 *  Function    :   PrintCmdLine
 *
 *  Parameters  :   none
 *
 *  Returns     :   nothing
 *
 *  Description :   Prints program usage
 *
 ********************************************************************/
static void PrintCmdLine(void)
{
	printf("\nUsage stor_scu remote_ae start stop -f filename -a local_ae -b local_port -c -n remote_host -p remote_port -l service_list -v -u username -w password -q\n");
	printf("\n");
	printf("\t remote_ae       name of remote Application Entity Title to connect with\n");
	printf("\t start           start image number (not required if -f specified)\n");
	printf("\t stop            stop image number (not required if -f specified)\n");
	printf("\t -f filename     (optional) specify a file containing a list of images to transfer\n");
	printf("\t -a local_ae     (optional) specify the local Application Title (default: MERGE_STORE_SCU)\n");
	printf("\t -b local_port   (optional) specify the local TCP listen port for commitment (default: found in the mergecom.pro file)\n");
	printf("\t -c              Do storage commitment for the transferred files\n");
	printf("\t -n remote_host  (optional) specify the remote hostname (default: found in the mergecom.app file for remote_ae)\n");
	printf("\t -p remote_port  (optional) specify the remote TCP listen port (default: found in the mergecom.app file for remote_ae)\n");
	printf("\t -l service_list (optional) specify the service list to use when negotiating (default: Storage_SCU_Service_List)\n");
	printf("\t -s              Transfer the data using stream (raw) mode\n");
	printf("\t -v              Execute in verbose mode, print negotiation information\n");
	printf("\t -u username     (optional) specify a username to negotiate as defined in DICOM Supplement 99\n");
	printf("\t                 Note: just a username can be specified, or a username and password can be specified\n");
	printf("\t                       A password alone cannot be specified.\n");
	printf("\t -w password     (optional) specify a password to negotiate as defined in DICOM Supplement 99\n");
	printf("\t -q              Positive response to user identity requested from SCP\n");
	printf("\n");
	printf("\tImage files must be in the current directory if -f is not used.\n");
	printf("\tImage files must be named 0.img, 1.img, 2.img, etc if -f is not used.\n");

} /* end PrintCmdLine() */


static void CopyHostName(int *i, char* A_argv[], STORAGE_OPTIONS* A_options)
{

	if (!strcmp(A_argv[*i], "-n") || !strcmp(A_argv[*i], "-N"))
	{
		/*
		 * Remote Host Name
		 */
		(*i)++;
		strcpy(A_options->RemoteHostname, A_argv[*i]);
	}

}
static void CopyPortName(int* i, char* A_argv[], STORAGE_OPTIONS* A_options)
{

	if (!strcmp(A_argv[*i], "-p") || !strcmp(A_argv[*i], "-P"))
	{
		/*
		 * Remote Port Number
		 */
		(*i)++;
		A_options->RemotePort = atoi(A_argv[*i]);

	}

}
static void CopyPatientSOPInstanceUID(int* i, char* A_argv[])
{
	if (!strcmp(A_argv[*i], "-SID"))
	{
		(*i)++;
		PatientDetails.SOPInstanceUID = A_argv[*i];

	}
}
static void CopyPatientFirstName(int *i, char* A_argv[])
{
	if (!strcmp(A_argv[*i], "-fname"))
	{
		(*i)++;
		PatientDetails.FirstName = A_argv[*i];
		
	}
	
}
static void CopyPatientLastName(int* i, char* A_argv[])
{
	if (!strcmp(A_argv[*i], "-lname"))
	{
		(*i)++;
		PatientDetails.LastName = A_argv[*i];
	}
}
static void CopyPatientId(int* i, char* A_argv[])
{
	if (!strcmp(A_argv[*i], "-id"))
	{
		(*i)++;
		PatientDetails.PatientId = A_argv[*i];
	}
}
static void CopyCmdArguments(int A_argc, char* A_argv[], STORAGE_OPTIONS* A_options)
{
	int       i = 0, argCount = 0;
	for (i = 1; i < A_argc; i++)
	{
		char c = (A_argv[i][0]);
		CopyHostName(&i, A_argv, A_options);
		CopyPortName(&i, A_argv, A_options);
		CopyPatientFirstName(&i, A_argv);
		CopyPatientLastName(&i, A_argv);
		CopyPatientSOPInstanceUID(&i, A_argv);
		CopyPatientId(&i, A_argv);
		if (!strcmp(A_argv[i], "-h"))
		{
			PrintCmdLine();
			//return SAMP_FALSE;
		}
		else 
		{
			/*
			 * Parse through the rest of the options
			 */

			argCount++;
			Parsethroughrestofoptions(argCount, &i, A_argv, A_options);

		}
	}
}
static void Parsethroughrestofoptions(int argCount, int* i, char* A_argv[], STORAGE_OPTIONS* A_options)
{
	/*switch (argCount)
	{
	case 1:
		strcpy(A_options->RemoteAE, A_argv[*i]);
		break;
	case 2:
		A_options->StartImage = A_options->StopImage = atoi(A_argv[*i]);
		break;

	default:
		A_options->StopImage = atoi(A_argv[*i]);
		break;

	}*/
	if (argCount == 1) {
		strcpy(A_options->RemoteAE, A_argv[*i]);
	}
	else {
		AssignStartStopOptions(argCount, A_argv, A_options, i);
	}

}
static void AssignStartStopOptions(int argCount, char* A_argv[], STORAGE_OPTIONS* A_options, int *i)
{
	switch (argCount) {
	case 2:
		A_options->StartImage = A_options->StopImage = atoi(A_argv[*i]);
		break;

	case 3:
		A_options->StopImage = atoi(A_argv[*i]);
		break;
	// default:
	// 	printf("Unkown option: %s\n", A_argv[*i]);
	// 	break;
	}
}

static void DefaultServiceList(STORAGE_OPTIONS* A_options)
{
	if (CheckHostDetails(A_options)
		&& !A_options->ServiceList[0])

	{
		strcpy(A_options->ServiceList, "Storage_SCU_Service_List");
	}
}
static int CheckHostDetails(STORAGE_OPTIONS* A_options)
{
	if (A_options->RemoteHostname[0] && (A_options->RemotePort != -1))
	{
		return 1;
	}
	return 0;
}
/*************************************************************************
 *
 *  Function    :   TestCmdLine
 *
 *  Parameters  :   Aargc   - Command line arguement count
 *                  Aargv   - Command line arguements
 *                  A_options - Local application options read in.
 *
 *  Return value:   SAMP_TRUE
 *                  SAMP_FALSE
 *
 *  Description :   Test command line for valid arguements.  If problems
 *                  are found, display a message and return SAMP_FALSE
 *
 *************************************************************************/
static SAMP_BOOLEAN TestCmdLine(int A_argc, char* A_argv[], STORAGE_OPTIONS* A_options)
{

	if (A_argc < 3)
	{
		PrintCmdLine();
		return SAMP_FALSE;
	}

	/*
	 * Set default values
	 */
	A_options->StartImage = 0;
	A_options->StopImage = 0;

	strcpy(A_options->LocalAE, "MERGE_STORE_SCU");
	strcpy(A_options->RemoteAE, "MERGE_STORE_SCP");
	strcpy(A_options->ServiceList, "Storage_SCU_Service_List");


	A_options->RemotePort = -1;
	A_options->RemoteHostname[0] = '\0';
	A_options->Verbose = SAMP_FALSE;
	A_options->StorageCommit = SAMP_FALSE;
	A_options->ListenPort = 1115;
	A_options->ResponseRequested = SAMP_FALSE;
	A_options->StreamMode = SAMP_FALSE;
	A_options->Username[0] = '\0';
	A_options->Password[0] = '\0';

	A_options->UseFileList = SAMP_FALSE;
	A_options->FileList[0] = '\0';

	/*
	 * Loop through each arguement
	 */
	CopyCmdArguments(A_argc, A_argv, A_options);

	/*
	 * If the hostname & port are specified on the command line,
	 * the user may not have the remote system configured in the
	 * mergecom.app file.  In this case, force the default service
	 * list, so we can attempt to make a connection, or else we would
	 * fail.
	 */
	 /*if ( A_options->RemoteHostname[0]
	 &&  !A_options->ServiceList[0]
	  && ( A_options->RemotePort != -1))
	 {
		 strcpy(A_options->ServiceList, "Storage_SCU_Service_List");
	 }*/
	DefaultServiceList(A_options);

	if (A_options->StopImage < A_options->StartImage)
	{
		printf("Image stop number must be greater than or equal to image start number.\n");
		PrintCmdLine();
		return SAMP_FALSE;
	}

	return SAMP_TRUE;

}/* TestCmdLine() */

static void update_list(InstanceNode** A_list, InstanceNode* newNode)
{
	InstanceNode* listNode;
	if (!*A_list)
	{
		/*

		  * Nothing in the list

		  */
		newNode->Next = *A_list;

		*A_list = newNode;

	}
	else
	{
		/*

		 * Add to the tail of the list

		 */
		listNode = *A_list;
		while (listNode->Next)
			listNode = listNode->Next;
		listNode->Next = newNode;

	}

}

/****************************************************************************
 *
 *  Function    :   AddFileToList
 *
 *  Parameters  :   A_list     - List of nodes.
 *                  A_fname    - The name of file to add to the list
 *
 *  Returns     :   SAMP_TRUE
 *                  SAMP_FALSE
 *
 *  Description :   Create a node in the instance list for a file to be sent
 *                  on this association.  The node is added to the end of the
 *                  list.
 *
 ****************************************************************************/
static SAMP_BOOLEAN AddFileToList(InstanceNode** A_list, char* A_fname)
{
	InstanceNode*    newNode;
	

	newNode = (InstanceNode*)malloc(sizeof(InstanceNode));
	if (!newNode)
	{
		PrintError("Unable to allocate object to store instance information", MC_NORMAL_COMPLETION);
		return (SAMP_FALSE);
	}

	memset(newNode, 0, sizeof(InstanceNode));

	strncpy(newNode->fname, A_fname, sizeof(newNode->fname));
	newNode->fname[sizeof(newNode->fname) - 1] = '\0';

	newNode->responseReceived = SAMP_FALSE;
	newNode->failedResponse = SAMP_FALSE;
	newNode->imageSent = SAMP_FALSE;
	newNode->msgID = -1;
	newNode->transferSyntax = IMPLICIT_LITTLE_ENDIAN;

	update_list(A_list, newNode);

	return (SAMP_TRUE);
}


/****************************************************************************
 *
 *  Function    :   UpdateNode
 *
 *  Parameters  :   A_node     - node to update
 *
 *  Returns     :   SAMP_TRUE
 *                  SAMP_FALSE
 *
 *  Description :   Update an image node with info about a file transferred
 *
 ****************************************************************************/
static SAMP_BOOLEAN UpdateNode(InstanceNode* A_node)
{
	MC_STATUS        mcStatus;

	/*
	 * Get DICOM msgID for tracking of responses
	 */
	mcStatus = MC_Get_Value_To_UInt(A_node->msgID, MC_ATT_MESSAGE_ID, &(A_node->dicomMsgID));
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		PrintError("MC_Get_Value_To_UInt for Message ID failed", mcStatus);
		A_node->responseReceived = SAMP_TRUE;
		return(SAMP_FALSE);
	}

	A_node->responseReceived = SAMP_FALSE;
	A_node->failedResponse = SAMP_FALSE;
	A_node->imageSent = SAMP_TRUE;

	return (SAMP_TRUE);
}


/****************************************************************************
 *
 *  Function    :   FreeList
 *
 *  Parameters  :   A_list     - Pointer to head of node list to free.
 *
 *  Returns     :   nothing
 *
 *  Description :   Free the memory allocated for a list of nodesransferred
 *
 ****************************************************************************/
static void FreeList(InstanceNode**    A_list)
{
	InstanceNode*    node;

	/*
	 * Free the instance list
	 */
	while (*A_list)
	{
		node = *A_list;
		*A_list = node->Next;

		if (node->msgID != -1)
			MC_Free_Message(&node->msgID);

		free(node);
	}
}


/****************************************************************************
 *
 *  Function    :   GetNumNodes
 *
 *  Parameters  :   A_list     - Pointer to head of node list to get count for
 *
 *  Returns     :   int, num node entries in list
 *
 *  Description :   Gets a count of the current list of instances.
 *
 ****************************************************************************/
static int GetNumNodes(InstanceNode*       A_list)

{
	int            numNodes = 0;
	InstanceNode*  node;

	node = A_list;
	while (node)
	{
		numNodes++;
		node = node->Next;
	}

	return numNodes;
}



/****************************************************************************
 *
 *  Function    :   ReadImage
 *
 *  Parameters  :   A_options  - Pointer to structure containing input
 *                               parameters to the application
 *                  A_appID    - Application ID registered
 *                  A_node     - The node in our list of instances
 *
 *  Returns     :   SAMP_TRUE
 *                  SAMP_FALSE
 *
 *  Description :   Determine the format of a DICOM file and read it into
 *                  memory.  Note that in a production application, the
 *                  file format should be predetermined (and not have to be
 *                  "guessed" by the CheckFileFormat function).  The
 *                  format for this application was chosen to show how both
 *                  DICOM Part 10 format files and "stream" format objects
 *                  can be sent over the network.
 *
 ****************************************************************************/
static void CheckFormat(FORMAT_ENUM format, SAMP_BOOLEAN* sampBool, InstanceNode* A_node, STORAGE_OPTIONS* A_options, int A_appID)
{
	switch (format)
	{
	/*case MEDIA_FORMAT:
		A_node->mediaFormat = SAMP_TRUE;
		*sampBool = ReadFileFromMedia(A_options, A_appID, A_node->fname, &A_node->msgID, &A_node->transferSyntax, &A_node->imageBytes);
		break;*/

	case IMPLICIT_LITTLE_ENDIAN_FORMAT:
	/*case IMPLICIT_BIG_ENDIAN_FORMAT:
	case EXPLICIT_LITTLE_ENDIAN_FORMAT:
	case EXPLICIT_BIG_ENDIAN_FORMAT:*/
		A_node->mediaFormat = SAMP_FALSE;
		*sampBool = ReadMessageFromFile(A_options, A_node->fname, format, &A_node->msgID, &A_node->transferSyntax, &A_node->imageBytes);
		break;

	case UNKNOWN_FORMAT:
		PrintError("Unable to determine the format of file", MC_NORMAL_COMPLETION);
		*sampBool = SAMP_FALSE;
		break;
	}
}
static void ReadSOPClassUID(InstanceNode* A_node)
{
	MC_STATUS mcStatus;
	mcStatus = MC_Get_Value_To_String(A_node->msgID, MC_ATT_SOP_CLASS_UID, sizeof(A_node->SOPClassUID), A_node->SOPClassUID);
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		PrintError("MC_Get_Value_To_String for SOP Class UID failed", mcStatus);
	}
}
static void ReadSOPInstanceUID(InstanceNode* A_node)
{
	MC_STATUS mcStatus;
	mcStatus = MC_Get_Value_To_String(A_node->msgID, MC_ATT_SOP_INSTANCE_UID, sizeof(A_node->SOPInstanceUID), A_node->SOPInstanceUID);
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		PrintError("MC_Get_Value_To_String for SOP Instance UID failed", mcStatus);
	}

}
//(0008,0018)	SOPInstanceUID	UI	1	44	2.16.840.1.113669.11.0.0.20201004.2143131358
static void ChangeSOPInstanceUID(InstanceNode* A_node) 
{
	MC_STATUS mcStatus;

	mcStatus = MC_Set_Value_From_String(A_node->msgID, MC_ATT_SOP_INSTANCE_UID, PatientDetails.SOPInstanceUID);
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		PrintError("MC_Set_Value_To_String for Patient Name failed", mcStatus);
	}
}
static void ChangePatientName(InstanceNode* A_node)
{
	MC_STATUS mcStatus;

	mcStatus = MC_Set_Value_From_String(A_node->msgID, MC_ATT_PATIENTS_NAME, strcat(strcat(PatientDetails.FirstName," "),PatientDetails.LastName));
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		PrintError("MC_Set_Value_To_String for Patient Name failed", mcStatus);
	}
}
static void ChangePatientID(InstanceNode* A_node)
{
	MC_STATUS mcStatus;
	mcStatus = MC_Set_Value_From_String(A_node->msgID, MC_ATT_PATIENT_ID, PatientDetails.PatientId);
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		PrintError("MC_Set_Value_To_String for Patient id failed", mcStatus);
	}
}

static SAMP_BOOLEAN ReadImage(STORAGE_OPTIONS* A_options, int A_appID, InstanceNode* A_node)
{
	FORMAT_ENUM             format = UNKNOWN_FORMAT;
	SAMP_BOOLEAN            sampBool = SAMP_FALSE;


	format = IMPLICIT_LITTLE_ENDIAN_FORMAT;//CheckFileFormat(A_node->fname);
	CheckFormat(format, &sampBool, A_node, A_options, A_appID);


	if (sampBool == SAMP_TRUE)
	{
		ReadSOPClassUID(A_node);
		ReadSOPInstanceUID(A_node);
		ChangeSOPInstanceUID(A_node);
		ChangePatientName(A_node);
		ChangePatientID(A_node);
	}
	fflush(stdout);
	return sampBool;
}

/****************************************************************************
 *
 *  Function    :   SendImage
 *
 *  Parameters  :   A_options  - Pointer to structure containing input
 *                               parameters to the application
 *                  A_associationID - Association ID registered
 *                  A_node     - The node in our list of instances
 *
 *  Returns     :   SAMP_TRUE
 *                  SAMP_FALSE on failure where association must be aborted
 *
 *  Description :   Send message containing the image in the node over
 *                  the association.
 *
 *                  SAMP_TRUE is returned on success, or when a recoverable
 *                  error occurs.
 *
 ****************************************************************************/

static SAMP_BOOLEAN SendImage(STORAGE_OPTIONS* A_options, int A_associationID, InstanceNode* A_node)
{
	MC_STATUS mcStatus;
	SAMP_BOOLEAN sampBool = SAMP_TRUE;



	A_node->imageSent = SAMP_FALSE;



	/* Get the SOP class UID and set the service */
	mcStatus = MC_Get_MergeCOM_Service(A_node->SOPClassUID, A_node->serviceName, sizeof(A_node->serviceName));



	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		PrintError("MC_Get_MergeCOM_Service failed", mcStatus);
		fflush(stdout);
		return (SAMP_TRUE);
	}



	mcStatus = MC_Set_Service_Command(A_node->msgID, A_node->serviceName, C_STORE_RQ);
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		PrintError("MC_Set_Service_Command failed", mcStatus);
		fflush(stdout);
		return (SAMP_TRUE);
	}



	/* set affected SOP Instance UID */
	mcStatus = MC_Set_Value_From_String(A_node->msgID, MC_ATT_AFFECTED_SOP_INSTANCE_UID, A_node->SOPInstanceUID);



	sampBool = SendImageSetSOPInstanceUID(mcStatus);
	
	sampBool = SendImageRequestMessage(A_options, A_associationID, A_node);






	A_node->imageSent = SAMP_TRUE;
	fflush(stdout);



	return sampBool;
}

static SAMP_BOOLEAN SendImageSetSOPInstanceUID(MC_STATUS mcStatus)
{
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		PrintError("MC_Set_Value_From_String failed for affected SOP Instance UID", mcStatus);
		fflush(stdout);
		return (SAMP_TRUE);
	}



	return SAMP_FALSE;
}

int FindErrorChecker(int* a,MC_STATUS mcStatus)
{
	int i = 0;
	while (a[i]) {
		if (mcStatus == a[i]) {
			return 1;
		}
		i++;
	}
	return 0;
}

static SAMP_BOOLEAN SendImageRequestMessage(STORAGE_OPTIONS* A_options, int A_associationID, InstanceNode* A_node)
{
	MC_STATUS mcStatus;
	mcStatus = MC_Send_Request_Message(A_associationID, A_node->msgID);
	int ErrorChecker[] = { MC_ASSOCIATION_ABORTED ,MC_SYSTEM_ERROR ,MC_UNACCEPTABLE_SERVICE };
	//(mcStatus == MC_ASSOCIATION_ABORTED) || (mcStatus == MC_SYSTEM_ERROR) || (mcStatus == MC_UNACCEPTABLE_SERVICE)
	if (FindErrorChecker(ErrorChecker,mcStatus))
	{
		/* At this point, the association has been dropped, or we should drop it in the case of error. */
		PrintError("MC_Send_Request_Message failed", mcStatus);
		fflush(stdout);
		return (SAMP_FALSE);
	}
	else if (mcStatus != MC_NORMAL_COMPLETION)
	{
		/* This is a failure condition we can continue with */
		PrintError("Warning: MC_Send_Request_Message failed", mcStatus);
		fflush(stdout);
		return (SAMP_TRUE);
	}



	return SAMP_TRUE;
}

/****************************************************************************
 *
 *  Function    :   ReadMessageFromFile
 *
 *  Parameters  :   A_options  - Pointer to structure containing input
 *                               parameters to the application
 *                  A_filename - Name of file to open
 *                  A_format   - Enum containing the format of the object
 *                  A_msgID    - The message ID of the message to be opened
 *                               returned here.
 *                  A_syntax   - The transfer syntax read is returned here.
 *                  A_bytesRead- Total number of bytes read in image.  Used
 *                               only for display and to calculate the
 *                               transfer rate.
 *
 *  Returns     :   SAMP_TRUE  on success
 *                  SAMP_FALSE on failure to open the file
 *
 *  Description :   This function reads a file in the DICOM "stream" format.
 *                  This format contains no DICOM part 10 header information.
 *                  The transfer syntax of the object is contained in the
 *                  A_format parameter.
 *
 *                  When this function returns failure, the caller should
 *                  not do any cleanup, A_msgID will not contain a valid
 *                  message ID.
 *
 ****************************************************************************/
static SAMP_BOOLEAN ReadMessageFromFile(STORAGE_OPTIONS* A_options,
	char* A_filename,
	FORMAT_ENUM A_format,
	int* A_msgID,
	TRANSFER_SYNTAX* A_syntax,
	size_t* A_bytesRead)
{



	MC_STATUS mcStatus;
	unsigned long errorTag = 0;
	CBinfo callbackInfo = { 0 };
	int retStatus = 0;
	SAMP_BOOLEAN sampBool;



	/*
	* Determine the format
	*/
	/*switch (A_format)
	{
	case IMPLICIT_LITTLE_ENDIAN_FORMAT:
	*A_syntax = IMPLICIT_LITTLE_ENDIAN;
	break;
	case IMPLICIT_BIG_ENDIAN_FORMAT:
	*A_syntax = IMPLICIT_BIG_ENDIAN;
	break;
	case EXPLICIT_LITTLE_ENDIAN_FORMAT:
	*A_syntax = EXPLICIT_LITTLE_ENDIAN;
	break;
	case EXPLICIT_BIG_ENDIAN_FORMAT:
	*A_syntax = EXPLICIT_BIG_ENDIAN;
	break;
	default:
	return SAMP_FALSE;
	}*/



	* A_syntax = IMPLICIT_LITTLE_ENDIAN;



	//if (A_options->Verbose)
	// printf("Reading DICOM \"stream\" format file in %s: %s\n", GetSyntaxDescription(*A_syntax), A_filename);



	/*
	* Open an empty message object to load the image into
	*/

	sampBool = ReadMessageFromFileEmptyMessage(A_msgID);
	/*mcStatus = MC_Open_Empty_Message(A_msgID);
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
	PrintError("Unable to open empty message", mcStatus);
	fflush(stdout);
	return SAMP_FALSE;
	}*/



	/*
	* Open and stream message from file
	*/



	//sampBool = ReadMessageFromFileOpenNStream(A_filename, A_msgID,callbackInfo );
	callbackInfo.fp = fopen(A_filename, BINARY_READ);
	//retStatus = setvbuf(callbackInfo.fp, (char*)NULL, _IOFBF, 32768);



	sampBool = ReadMessageFromFileOpenNStream(A_filename, A_msgID);



	/*if (!callbackInfo.fp)
	{
	printf("ERROR: Unable to open %s.\n", A_filename);
	MC_Free_Message(A_msgID);
	fflush(stdout);
	return SAMP_FALSE;
	}



	retStatus = setvbuf(callbackInfo.fp, (char*)NULL, _IOFBF, 32768);
	if (retStatus != 0)
	{
	printf("WARNING: Unable to set IO buffering on input file.\n");
	}*/



	//sampBool = ReadMessageFromFileBufferAllocate(A_filename);



	if (callbackInfo.bufferLength == 0)
	{
		int length = 0;



		mcStatus = MC_Get_Int_Config_Value(WORK_BUFFER_SIZE, &length);
		if (mcStatus != MC_NORMAL_COMPLETION)
		{
			length = WORK_SIZE;
		}
		callbackInfo.bufferLength = length;
	}




	callbackInfo.buffer = malloc(callbackInfo.bufferLength);



	sampBool = ReadMessageFromFileBufferAllocate(callbackInfo);
	/*if (callbackInfo.buffer == NULL)
	{
	printf("ERROR: failed to allocate file read buffer [%d] kb", (int)callbackInfo.bufferLength);
	fflush(stdout);
	return SAMP_FALSE;
	}*/



	mcStatus = MC_Stream_To_Message(*A_msgID, 0x00080000, 0xFFFFFFFF, *A_syntax, &errorTag, (void*)&callbackInfo, StreamToMsgObj);



	ReadMessageFromFileClose(callbackInfo);



	/*if (callbackInfo.fp)
	fclose(callbackInfo.fp);



	if (callbackInfo.buffer)
	free(callbackInfo.buffer);*/



	sampBool = ReadMessageFromFileStreamError(mcStatus, A_msgID);



	/*if (mcStatus != MC_NORMAL_COMPLETION)
	{
	PrintError("MC_Stream_To_Message error, possible wrong transfer syntax guessed", mcStatus);
	MC_Free_Message(A_msgID);
	fflush(stdout);
	return SAMP_FALSE;
	}*/



	* A_bytesRead = callbackInfo.bytesRead;
	fflush(stdout);



	return SAMP_TRUE;



} /* ReadMessageFromFile() */

static SAMP_BOOLEAN ReadMessageFromFileEmptyMessage(int* A_msgID)
{
	MC_STATUS mcStatus;
	//SAMP_BOOLEAN sampBool;



	mcStatus = MC_Open_Empty_Message(A_msgID);
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		PrintError("Unable to open empty message", mcStatus);
		fflush(stdout);
		return SAMP_FALSE;
	}



	return SAMP_TRUE;
}



static SAMP_BOOLEAN ReadMessageFromFileOpenNStream(char* A_filename, int* A_msgID)
{
	CBinfo callbackInfo = { 0 };
	int retStatus = 0;
	//SAMP_BOOLEAN sampBool;



	callbackInfo.fp = fopen(A_filename, BINARY_READ);
	if (!callbackInfo.fp)
	{
		printf("ERROR: Unable to open %s.\n", A_filename);
		MC_Free_Message(A_msgID);
		fflush(stdout);
		return SAMP_FALSE;
	}



	retStatus = setvbuf(callbackInfo.fp, (char*)NULL, _IOFBF, 32768);
	if (retStatus != 0)
	{
		printf("WARNING: Unable to set IO buffering on input file.\n");
	}



	return SAMP_TRUE;
}



static SAMP_BOOLEAN ReadMessageFromFileBufferAllocate(CBinfo callbackInfo)
{

	callbackInfo.buffer = malloc(callbackInfo.bufferLength);
	if (callbackInfo.buffer == NULL)
	{
		printf("ERROR: failed to allocate file read buffer [%d] kb", (int)callbackInfo.bufferLength);
		fflush(stdout);
		return SAMP_FALSE;
	}



	return SAMP_TRUE;
}



static SAMP_BOOLEAN ReadMessageFromFileStreamError(MC_STATUS mcStatus, int* A_msgID)
{
	if (mcStatus != MC_NORMAL_COMPLETION)
	{
		PrintError("MC_Stream_To_Message error, possible wrong transfer syntax guessed", mcStatus);
		MC_Free_Message(A_msgID);
		fflush(stdout);
		return SAMP_FALSE;
	}



	return SAMP_TRUE;
}



static void ReadMessageFromFileClose(CBinfo callbackInfo)
{
	if (callbackInfo.fp)
		fclose(callbackInfo.fp);



	if (callbackInfo.buffer)
		free(callbackInfo.buffer);
}

/*************************************************************************
 *
 *  Function    :  StreamToMsgObj
 *
 *  Parameters  :  A_msgID         - Message ID of message being read
 *                 A_CBinformation - user information passwd to callback
 *                 A_isFirst       - flag to tell if this is the first call
 *                 A_dataSize      - length of data read
 *                 A_dataBuffer    - buffer where read data is stored
 *                 A_isLast        - flag to tell if this is the last call
 *
 *  Returns     :  MC_NORMAL_COMPLETION on success
 *                 any other return value on failure.
 *
 *  Description :  Reads input stream data from a file, and places the data
 *                 into buffer to be used by the MC_Stream_To_Message function.
 *
 **************************************************************************/
void CallBackInfoFP(int* A_isLast,CBinfo* callbackInfo)
{
	if (feof(callbackInfo->fp))
	{
		*A_isLast = 1;
		fclose(callbackInfo->fp);
		callbackInfo->fp = NULL;
	}
	else
		*A_isLast = 0;
}
static MC_STATUS NOEXP_FUNC StreamToMsgObj(int        A_msgID,
	void*      A_CBinformation,
	int        A_isFirst,
	int*       A_dataSize,
	void**     A_dataBuffer,
	int*       A_isLast)
{
	size_t          bytesRead;
	CBinfo*         callbackInfo = (CBinfo*)A_CBinformation;

	if (A_isFirst)
		callbackInfo->bytesRead = 0L;

	bytesRead = fread(callbackInfo->buffer, 1, callbackInfo->bufferLength, callbackInfo->fp);
	if (ferror(callbackInfo->fp))
	{
		perror("\tRead error when streaming message from file.\n");
		return MC_CANNOT_COMPLY;
	}

	/*if (feof(callbackInfo->fp))
	{
		*A_isLast = 1;
		fclose(callbackInfo->fp);
		callbackInfo->fp = NULL;
	}
	else
		*A_isLast = 0;*/
	CallBackInfoFP(A_isLast, callbackInfo);

	*A_dataBuffer = callbackInfo->buffer;
	*A_dataSize = (int)bytesRead;
	callbackInfo->bytesRead += bytesRead;

	return MC_NORMAL_COMPLETION;
} /* StreamToMsgObj() */


/****************************************************************************
 *
 *  Function    :   Create_Inst_UID
 *
 *  Parameters  :   none
 *
 *  Returns     :   A pointer to a new UID
 *
 *  Description :   This function creates a new UID for use within this
 *                  application.  Note that this is not a valid method
 *                  for creating UIDs within DICOM because the "base UID"
 *                  is not valid.
 *                  UID Format:
 *                  <baseuid>.<deviceidentifier>.<serial number>.<process id>
 *                       .<current date>.<current time>.<counter>
 *
 ****************************************************************************/
static char * Create_Inst_UID()
{
	static short UID_CNTR = 0;
	static char  deviceType[] = "1";
	static char  serial[] = "1";
	static char  Sprnt_uid[68];
	char         creationDate[68];
	char         creationTime[68];
	time_t       timeReturn;
	struct tm*   timePtr;
#ifdef UNIX
	unsigned long pid = getpid();
#endif


	timeReturn = time(NULL);
	timePtr = localtime(&timeReturn);

	sprintf(creationDate, "%d%d%d", (timePtr->tm_year + 1900), (timePtr->tm_mon + 1), timePtr->tm_mday);
	sprintf(creationTime, "%d%d%d", timePtr->tm_hour, timePtr->tm_min, timePtr->tm_sec);

#ifdef UNIX
	sprintf(Sprnt_uid, "2.16.840.1.999999.%s.%s.%d.%s.%s.%d", deviceType, serial, pid, creationDate, creationTime, UID_CNTR++);
#else
	sprintf(Sprnt_uid, "2.16.840.1.999999.%s.%s.%s.%s.%d", deviceType, serial, creationDate, creationTime, UID_CNTR++);
#endif

	return(Sprnt_uid);
}


/****************************************************************************
 *
 *  Function    :   PrintError
 *
 *  Description :   Display a text string on one line and the error message
 *                  for a given error on the next line.
 *
 ****************************************************************************/
static void PrintError(char* A_string, MC_STATUS A_status)
{
	char        prefix[30] = { 0 };
	/*
	 *  Need process ID number for messages
	 */
#ifdef UNIX
	sprintf(prefix, "PID %d", getpid());
#endif
	if (A_status == -1)
	{
		printf("%s\t%s\n", prefix, A_string);
	}
	else
	{
		printf("%s\t%s:\n", prefix, A_string);
		printf("%s\t\t%s\n", prefix, MC_Error_Message(A_status));
	}
}


