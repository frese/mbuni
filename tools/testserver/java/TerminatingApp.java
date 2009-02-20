/*
 * @(#)TerminatingApp.java	1.1
 *
 * Copyright (c) Nokia Corporation 2002
 *
 */

import java.io.*;
import java.util.*;
import java.net.*;
import com.nokia.mms.*;


public class TerminatingApp {
  final static int SYNC_MODE = 0;
  final static int ASYNC_MODE = 1;
  final static int RESPONSE_DELAY = 10000;
  final static int HOST_PORT = 8189;
  private String requestType;
  private Hashtable headers;
  private DataInputStream in;
  private DataOutputStream out;
  private int mode;
  private String client;

  public TerminatingApp(int aPort, int aMode) {
    try {
      mode = aMode;
      headers = new Hashtable();
      // Creates a server socket on the specified port,
      // waiting for a request to come in over the network.
      ServerSocket s = new ServerSocket(aPort);
      System.out.println("Terminating Application is listening on port " + aPort);
      System.out.print("Application Mode: ");
      if (mode == SYNC_MODE)
        System.out.println("Synchronous");
      else
        System.out.println("Asynchronous");

      // Listens for a connection to be made to this socket and accepts it.
      // The method blocks until a connection is made.
      Socket incoming = s.accept( );
      in = new DataInputStream(incoming.getInputStream());
      out = new DataOutputStream(incoming.getOutputStream());

      client = incoming.getInetAddress().getHostAddress() + ":" + HOST_PORT;
      System.out.println("New message received from " + client);

      String response;
      // Decodes the message and returns a response to the sender.
      boolean success = parseRequest();
      if (success)
        response = "HTTP/1.1 " + (mode == SYNC_MODE ? "204 No Content" : "202 Accepted") + "\r\n";
      else
        response = "HTTP/1.1 400 Message Validation Failed\r\n";

      out.writeBytes(response);
      out.flush();
      out.close();
      in.close();
      incoming.close();

      if (success && mode == ASYNC_MODE) {
        Thread.sleep(RESPONSE_DELAY);
        sendAsyncResponse();
      }

    } catch (Exception e) {
      System.out.println(e.getMessage());
    }
  }

  private void sendAsyncResponse() {
    try {
      URL url = new URL("http://" + client);
      HttpURLConnection uc = (HttpURLConnection) url.openConnection();
      uc.setDoInput(true);
      uc.setDoOutput(true);
      uc.setUseCaches(false);
      uc.setRequestProperty("X-NOKIA-MMSC-Message-Id", (String)headers.get("x-nokia-mmsc-message-id"));
      uc.setRequestProperty("X-NOKIA-MMSC-Message-Type", "MultiMediaMessage");
      uc.setRequestProperty("X-NOKIA-MMSC-Status", "200");

      DataOutputStream dos = new DataOutputStream(uc.getOutputStream());
      dos.writeBytes("");
      dos.flush();
      dos.close();

      System.out.println("HTTP/1.1 " + uc.getResponseCode() + " " + uc.getResponseMessage());
      System.out.println("X-NOKIA-MMSC-Version: " + uc.getHeaderField("X-NOKIA-MMSC-Version"));

    } catch (Exception e) {
      System.out.println(e.getMessage());
    }
  }

  private boolean parseRequest() {
    byte[] match = {13,10,13,10};
    int pos, ch, contentLength;
    int index = 0;
    String line = "";

    try {
      while ((ch = in.read()) >= 0) {
        if (ch == match[index] && index <= 3)
          index++;
        else
          index = 0;



        if (index == 4) {
          // Gets the request content.
          contentLength = Integer.parseInt((String)headers.get("content-length"));
          byte[] content = new byte[contentLength];

          for (int j = 0; j < contentLength; j++)
            content[j] = (byte) in.read();

		  printRawMessage(content);

          // Decodes the multimedia message.
          MMDecoder mmDecoder = new MMDecoder();
          mmDecoder.setMessage(content);
          mmDecoder.decodeMessage();
          MMMessage mm = mmDecoder.getMessage();
          // Prints the message details.
          printMessage(mm);
          // Saves as file each multimedia content included within the message.
          storeContents(mm);

          break;
        }

        // Gets the request headers
        line += (char) ch;
        if ((line.length() > 2) && (ch == '\n')) {
          pos = line.indexOf(':');
          if (pos != -1)
            setHeader(line.substring(0, pos).toLowerCase(), line.substring(pos + 2, line.length()-2));
          else
            setRequestType(line.substring(0, line.length()-2));
          line = "";
        }
      }
      return true;
    } catch (Exception e) {
      System.out.println(e.getMessage());
      return false;
    }

  }

  private void setHeader(String key, String value) {
    headers.put(key.toLowerCase(), value);
    System.out.println(key + ": " + value);
  }

  private void setRequestType(String s) {
    requestType = s;
    System.out.println(s);
  }

  private void printRawMessage(byte[] content) {
		System.out.println("Raw message contents: [" + HexUtil.byteToHex(content) + "]");
  }



  private void printMessage(MMMessage mm) {
    if (mm.getMessageType() == IMMConstants.MESSAGE_TYPE_M_SEND_REQ)
      System.out.println(" Message Type: m-send-req");

    System.out.println(" Transaction Id: " + mm.getTransactionId());
    System.out.println(" Message Id: " + mm.getMessageId());
    System.out.println(" Message Status: " + unsignedByte(mm.getMessageStatus()));
    System.out.println(" Version: " + mm.getVersionAsString());

    if (mm.isToAvailable()) {
      Vector list = mm.getTo();
      for (int n=0; n < list.size(); n++) {
        MMAddress to = (MMAddress)list.elementAt(n);
        System.out.println(" TO (" + n + "): " + to.getAddress() + " (type=" + to.getType() + ")");
      }
    }

    if (mm.isDeliveryReportAvailable())
     System.out.println(" Delivery Report: " + mm.getDeliveryReport());

    if (mm.isReadReplyAvailable())
     System.out.println(" Read Reply: "+mm.getReadReply());

    if (mm.isCcAvailable()) {
      Vector list=mm.getCc();
      for (int n = 0; n < list.size(); n++) {
        MMAddress cc = (MMAddress)list.elementAt(n);
        System.out.println(" CC (" + n + "): " + cc.getAddress() + " (type=" + cc.getType() + ")");
      }
    }

    if (mm.isBccAvailable()) {
      Vector list = mm.getBcc();
      for (int n=0; n < list.size(); n++) {
        MMAddress bcc = (MMAddress)list.elementAt(n);
        System.out.println(" BCC (" + n + "): " + bcc.getAddress() + " (type=" + bcc.getType() + ")");
      }
    }

    if (mm.isDateAvailable())
      System.out.println(" Date: " + mm.getDate());

    if (mm.isFromAvailable()) {
      MMAddress from = new MMAddress( mm.getFrom() );
      System.out.println(" From: " + from.getAddress() + " (type=" + from.getType() +")");
    }

    if (mm.isSubjectAvailable())
      System.out.println(" Subject: " + mm.getSubject());

    if (mm.isMessageClassAvailable()) {
      if (mm.getMessageClass()==IMMConstants.MESSAGE_CLASS_PERSONAL)
        System.out.println(" Message Class: MESSAGE_CLASS_PERSONAL");
      else if (mm.getMessageClass()==IMMConstants.MESSAGE_CLASS_ADVERTISEMENT)
        System.out.println(" Message Class: MESSAGE_CLASS_ADVERTISEMENT");
      else if (mm.getMessageClass()==IMMConstants.MESSAGE_CLASS_INFORMATIONAL)
        System.out.println(" Message Class: MESSAGE_CLASS_INFORMATIONAL");
      else if (mm.getMessageClass()==IMMConstants.MESSAGE_CLASS_AUTO)
        System.out.println(" Message Class: MESSAGE_CLASS_AUTO");
      else
        System.out.println(" Message Class: <unknown> ("+mm.getMessageClass()+")");
    }

    if (mm.isExpiryAvailable()) {
      if (mm.isExpiryAbsolute())
        System.out.println(" Expiry time: " + mm.getExpiry());
      else {
        if (mm.isDateAvailable())
          System.out.println(" Expiry time: " + (new Date(mm.getDate().getTime()+mm.getExpiry().getTime())));
        else
          System.out.println(" Expiry time: " + (new Date(mm.getExpiry().getTime())));
      }
    }

    if (mm.isDeliveryTimeAvailable()) {
      if (mm.isDeliveryTimeAbsolute())
        System.out.println(" Delivery time: " + mm.getDeliveryTime());
      else {
        Date date=new Date(mm.getDate().getTime()+mm.getDeliveryTime().getTime());
        System.out.println(" Delivery time: " + date );
      }
    }

    if (mm.isPriorityAvailable()) {
      if (mm.getPriority()==IMMConstants.PRIORITY_LOW)
        System.out.println(" Priority: PRIORITY_LOW");
      else if (mm.getPriority()==IMMConstants.PRIORITY_NORMAL)
        System.out.println(" Priority: PRIORITY_NORMAL");
      else if (mm.getPriority()==IMMConstants.PRIORITY_HIGH)
        System.out.println(" Priority: PRIORITY_HIGH");
    }

    if (mm.isContentTypeAvailable())
      System.out.println(" Content Type: " + mm.getContentType());

    if (mm.isPresentationAvailable())
      System.out.println(" MultipartRelatedType: " + mm.getMultipartRelatedType());

    System.out.println(" PresentationId: " + mm.getPresentationId());
    System.out.println(" Number Of Contents: " + mm.getNumContents());

  }

  private void storeContents(MMMessage mm) throws IOException {
    MMContent mmContent = null;
    String path = getPath();

    for (int j=0; j<mm.getNumContents(); j++) {
      mmContent = mm.getContent(j);
      System.out.println(" Content " + j + " --> ID = " +
        mmContent.getContentId() + ", TYPE = " + mmContent.getType() +
        ", LENGTH = " + mmContent.getLength());

      if ((mmContent.getType()).compareTo(IMMConstants.CT_IMAGE_WBMP)==0)
        mmContent.saveToFile(path + cleanString(mmContent.getContentId())+".wbmp");
      else if ((mmContent.getType()).compareTo(IMMConstants.CT_TEXT_PLAIN)==0)
        mmContent.saveToFile(path + cleanString(mmContent.getContentId())+".txt");
      else if ((mmContent.getType()).compareTo(IMMConstants.CT_APPLICATION_SMIL)==0)
        mmContent.saveToFile(path + cleanString(mmContent.getContentId())+".smil");
      else if ((mmContent.getType()).compareTo(IMMConstants.CT_IMAGE_JPEG)==0)
        mmContent.saveToFile(path + cleanString(mmContent.getContentId())+".jpg");
      else if ((mmContent.getType()).compareTo(IMMConstants.CT_IMAGE_GIF)==0)
        mmContent.saveToFile(path + cleanString(mmContent.getContentId())+".gif");
      else if ((mmContent.getType()).compareTo("audio/wav")==0)
        mmContent.saveToFile(path + cleanString(mmContent.getContentId())+".wav");
      else
        mmContent.saveToFile(path + cleanString(mmContent.getContentId()));
    }
  }

  private String cleanString(String str) {
    String result;

    if (  (str.charAt(0)=='<') && (str.charAt(str.length()-1)=='>')  )
       result = str.substring(1,str.length()-1);
    else
       result = str;
    return result;
  }

  private int unsignedByte(byte value) {
    if (value<0) {
      return (value+256);
    } else
      return value;
  }

  private String getPath() {
    URL url = getClass().getResource(getClass().getName() + ".class");
    String classPath = url.getHost() + url.getFile();
    int pos = classPath.lastIndexOf("/");
    return classPath.substring(0, pos + 1);
  }

  public static void main (String[] args) {
    // Default port for creating the server socket.
    int port = 7000;
    int mode = SYNC_MODE;
//    int mode = ASYNC_MODE;

    if (args.length > 0) {
      try {
        port = Integer.parseInt(args[0]);
      } catch (Exception e) {
        System.out.println("Invalid port specified: " + args[0]);
      }

      if (args.length > 1 && args[1].equalsIgnoreCase("A"))
        mode = ASYNC_MODE;
    }


    TerminatingApp ta = new TerminatingApp(port, mode);
  }

}