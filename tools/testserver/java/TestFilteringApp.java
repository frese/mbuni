import java.net.*;
import java.io.*;
import java.util.*;
import com.nokia.mms.*;


public class TestFilteringApp {

  private int port;
  private  String requestType;
  private  Hashtable headers;


  public TestFilteringApp(int port) {
    this.port = port;
  }

  public void goSyncApp(byte[] buf) {
    try {
      headers = new Hashtable();
      ServerSocket s = new ServerSocket(port);
      System.out.println("\n\nFiltering Application is listening on port " + port+" (Synchronous)");

      Socket incoming = s.accept( );
      DataInputStream in = new DataInputStream(incoming.getInputStream());
      DataOutputStream out = new DataOutputStream(incoming.getOutputStream());

      System.out.println("New message received:");

      byte[] content = null;
      byte[] match = {13,10,13,10};
      int pos, ch, contentLength;
      int index = 0;
      String line = "";


      while ((ch = in.read()) >= 0) {
        if (ch == match[index] && index <= 3)
          index++;
        else
          index = 0;

        // Reading messageBody
        if (index == 4) {
          contentLength = getContentLength();
          byte[] messageBody = new byte[contentLength];

          for (int j = 0; j < contentLength; j++)
            messageBody[j] = (byte) in.read();

          MMDecoder mmDecoder = new MMDecoder();
          mmDecoder.setMessage(messageBody);
          mmDecoder.decodeMessage();
          MMMessage mm = mmDecoder.getMessage();
          printMessage(mm);


          break;
        }

        // Reading headers
        line += (char) ch;
        if ((line.length() > 2) && (ch == '\n')) {
          pos = line.indexOf(':');
          if (pos != -1)
            setHeader(line.substring(0, pos), line.substring(pos + 2, line.length()-2));
          else
            setRequestType(line.substring(0, line.length()-2));
          line = "";
        }

      }
      if (buf==null)
        out.writeBytes(getSyncResponseNoBody());
      else
        out.writeBytes(getSyncResponseWithBody());
      if (buf!=null) {
        out.writeBytes("Content-Length:"+buf.length+"\r\n\r\n");
        System.out.println("Content-Length:"+buf.length+"\r\n\r\n");
      }

      if (buf!=null)
        out.write(buf);
      out.flush();
      out.close();
      in.close();
      incoming.close();

    } catch (Exception e) {
      System.out.println(e.getMessage());
    }
  }


  public String goAsyncReceiver() {
    String messageID="";
    try {
      headers = new Hashtable();
      ServerSocket s = new ServerSocket(port);
      System.out.println("\n\nFiltering Application is listening on port " + port+" (Asynchronous)");

      Socket incoming = s.accept( );
      DataInputStream in = new DataInputStream(incoming.getInputStream());
      DataOutputStream out = new DataOutputStream(incoming.getOutputStream());

      System.out.println("New message received:");

      byte[] content = null;
      byte[] match = {13,10,13,10};
      int pos, ch, contentLength;
      int index = 0;
      String line = "";


      while ((ch = in.read()) >= 0) {
        if (ch == match[index] && index <= 3)
          index++;
        else
          index = 0;

        // Reading messageBody
        if (index == 4) {
          contentLength = getContentLength();
          byte[] messageBody = new byte[contentLength];

          for (int j = 0; j < contentLength; j++)
            messageBody[j] = (byte) in.read();

          MMDecoder mmDecoder = new MMDecoder();
          mmDecoder.setMessage(messageBody);
          mmDecoder.decodeMessage();
          MMMessage mm = mmDecoder.getMessage();
          printMessage(mm);


          break;
        }

        // Reading headers
        line += (char) ch;
        if ((line.length() > 2) && (ch == '\n')) {
          pos = line.indexOf(':');
          if (pos != -1){
            setHeader(line.substring(0, pos), line.substring(pos + 2, line.length()-2));
            if (line.substring(0, pos).equals("x-nokia-mmsc-message-id"))
              messageID = line.substring(pos + 2, line.length()-2);
            }
          else
            setRequestType(line.substring(0, line.length()-2));
          line = "";
        }

      }

      out.writeBytes(getAsyncResponse_1());
      out.flush();
      out.close();
      in.close();
      incoming.close();


    } catch (Exception e) {
      System.out.println(e.getMessage());
      return "";
    }
   return messageID;
  }


  private int getContentLength() {
    try {
      return Integer.parseInt((String)headers.get("content-length"));
    } catch (Exception e) {
      return 0;
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

  private String getSyncResponseWithBody() {
    String s;
    s = "HTTP/1.1 200 OK\r\n";
    s += "Content-Type: application/vnd.wap.mms-message\r\n";
    s += "X-NOKIA-MMSC-Version: 1.1\r\n";
    System.out.println("Response with body:");
    System.out.println(s+"\n\n");

    return s;
  }

  private String getSyncResponseNoBody() {
    String s;
    s = "HTTP/1.1 204 No Content\r\n";
    System.out.println("Response no body:");
    System.out.println(s+"\n\n");

    return s;
  }

  private String getAsyncResponse_1() {
    String s;
    s = "HTTP/1.1 202 Accepted\r\n";
    return s;
  }



  private int unsignedByte(byte value) {
    if (value<0) {
      return (value+256);
    } else
      return value;
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

  private byte[] readFile(String filename) {
    int fileSize=0;
    RandomAccessFile fileH=null;
    // opens the file for reading
    try {
      fileH=new RandomAccessFile(filename, "r");
      fileSize=(int)fileH.length();
    } catch(IOException ioErr) {
      System.err.println("Cannot find " + filename);
      System.err.println(ioErr);
      System.exit(200);
    }

    System.out.println(" Reading file "+filename+"("+fileSize+" bytes)...\n");

  // allocates the buffer large enough to hold entire file
  byte [] buf = new byte[fileSize];

  // reads all bytes of file
    int i=0;
    try {
       while (true)
       {
         try
         {
           buf[i++]=fileH.readByte();
         }
         catch (EOFException e) {break;}
       }
    } catch (IOException ioErr) {System.out.println("ERROR in reading of file"+filename);};

    return buf;

  }

  private void goAsyncResponse_2(String strURL,String msgID,byte[] buf){
  try {

      System.out.println("\n\nSending Asynchronous Response");
      System.out.println("URL  : "+strURL);
      System.out.println("MsgID: "+msgID);


      URL url = new URL(strURL);
      HttpURLConnection uc = (HttpURLConnection) url.openConnection();
      uc.setDoInput(true);
      uc.setDoOutput(true);
      uc.setUseCaches(false);
      uc.setRequestProperty("X-NOKIA-MMSC-Message-Id", msgID);
      uc.setRequestProperty("X-NOKIA-MMSC-Message-Type", "MultiMediaMessage");
      uc.setRequestProperty("X-NOKIA-MMSC-Charging", "1");
      uc.setRequestProperty("X-NOKIA-MMSC-Status", "200");
      uc.setRequestProperty("Content-Type", "application/vnd.wap.mms-message");


      DataOutputStream dos = new DataOutputStream(uc.getOutputStream());
      if (buf!=null)
        dos.write(buf);
      else
        dos.writeBytes("");

      dos.flush();
      dos.close();

      System.out.println("Response code: " + uc.getResponseCode());
      System.out.println("Response message: " + uc.getResponseMessage());
      System.out.println("X-NOKIA-MMSC-Version: " + uc.getHeaderField("X-NOKIA-MMSC-Version"));

    } catch (Exception e) {
      System.out.println(e);
      e.printStackTrace();
    }

  }

  private String showMainMenu(){
    System.out.println("Filtering Application Testing Menu");
    System.out.println("\n1. Filtering Synchronous Application");
    System.out.println("2. Filtering Asynchronous Application");
    System.out.println("\n0. Exit");
    System.out.print("\n\nSelect application type: ");

    DataInputStream di = new DataInputStream(System.in);
    char ch;
    String cmd="";
    try {
      while ((ch=(char)di.read())!='\n'){
		  if ((ch!='\n') && (ch!='\r')) {
              cmd= cmd + ch;
		  }
      }
    } catch(Exception e){
      e.printStackTrace();
    }
   return new String(cmd);
  }

  private String showMenuResponseType(){
    System.out.println("Filtering Application Testing Menu");
    System.out.println("\n1. Status only");
    System.out.println("2. Status + Headers only");
    System.out.println("3. Status + Whole message");
    System.out.println("\n0. Exit");
    System.out.print("\n\nSelect response type: ");

    DataInputStream di = new DataInputStream(System.in);
    char ch;
    String cmd="";
    try {
      while ((ch=(char)di.read())!='\n'){
		  if ((ch!='\n') && (ch!='\r')) {
              cmd= cmd + ch;
		  }
      }
    } catch(Exception e){
      e.printStackTrace();
    }
   return new String(cmd);
  }

  public static void main(String[] args) {
    TestFilteringApp ta = new TestFilteringApp(7000);
    String cmd=ta.showMainMenu();
	String cmdRespType="";
    byte[] buf=null;
    if (cmd.equals("0")) {
      System.exit(200);
	}
    else {
      cmdRespType=ta.showMenuResponseType();
	}

    if (cmdRespType.equals("2")){
        buf = ta.readFile(".\\test_header_only.mms");
    }
    else  if (cmdRespType.equals("3")){
        buf = ta.readFile(".\\test.mms");
    }

    if (cmd.equals("1")){
      ta.goSyncApp(buf);
    }
    else if (cmd.equals("2")){
      String msgID = ta.goAsyncReceiver();
      System.out.println("MSG "+msgID);
      System.out.println("\n\nWaiting ...");
      try {
      Thread.sleep(10000);
      } catch(Exception e){
        e.printStackTrace();
      }
      ta.goAsyncResponse_2("http://127.0.0.1:8189",msgID,buf);
    }



  }
}