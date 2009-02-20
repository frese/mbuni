/*
 * @(#)SampleMMSCreation.java	1.0
 *
 *
 * This sample was derived from the OriginatingApp.java file which came with the
 * Nokia MMS Java Library 1.1
 *
 * Instead of sending the MMS to an MMSC, however, this sample simply writes the
 * encoded MMS to a file.  That file can then be used, e.g. with various tools
 * that are provided by Nokia -- the EAIF emulator, the Series60 terminal emulator
 * that comes with the Series60 SDK for Symbian OS, the 7210 simulator, etc...
 *
 * The sample is made as simple as possible while still pointing out key points
 * which are *not* explained in the samples that accompany the library.
 * For instance, many of the headers that were set in the original sample were set
 * to default values.  Those have been left out here for simplification.
 *
 * Note - this sample was changed with only limited knowledge of Java.  Java experts
 * are welcome to give feedback, and non-Java-experts may take heart that if we can
 * do it, you can too!
 *
 * Basic gist of this sample will be:
 * 1)Set headers
 * 2)Add various content parts
 * 3)Encode the message
 * 4)Write the encoded message to a file
 *
 *
 *
 * Change History
 * ----------------------------------------------------------------------------
 * * 25-09-2002 * Version 1.0 * Document added into ForumNokia                *
 * ----------------------------------------------------------------------------
 *
 *
 *
 * Copyright (c) Nokia Corporation 2002
 *
 */

import java.io.*;
import java.util.*;
import java.net.*;
import com.nokia.mms.*;

public class SampleMMSCreation {
  private static String datapath = "../data/";

  public SampleMMSCreation() {
    MMMessage mms = new MMMessage();
//1)Set headers
    SetHeaders(mms);
//2)Add various content parts
    AddContents(mms);

    MMEncoder encoder=new MMEncoder();
    encoder.setMessage(mms);

    try {
//3)Encode the message
      encoder.encodeMessage();
      byte[] out = encoder.getMessage();
//4)Print the encoded message to a file
      createMmsFile(out, "Sample.mms");
    }
    catch (Exception e) {
      System.out.println(e.getMessage());
    }
  }

  private void SetHeaders(MMMessage m) {
	//Just going to set the mandatories, and the subject...
	//Type,TransID,Version are all mandatory, and must be the first headers, in this order!
	m.setMessageType(IMMConstants.MESSAGE_TYPE_M_SEND_REQ);
    m.setTransactionId("0123456789");
    m.setVersion(IMMConstants.MMS_VERSION_10);

    m.setFrom("+123/TYPE=PLMN");
    m.addToAddress("+456/TYPE=PLMN"); //at least one To/CC/BCC is mandatory

    m.setSubject("My first test message!"); //subject is optional

    //ContentType is mandatory, and must be last header!  These last 3 lines set the ContentType to
    //application/vnd.wml.multipart.related;type="application/smil";start="<0000>"
    //In case of multipart.mixed, only the first line is needed (and change the constant)

    //Any string will do for the Content-ID, but it must match that used for the presentation part,
    //and it must be Content-ID, it cannot be Content-Location.

    m.setContentType(IMMConstants.CT_APPLICATION_MULTIPART_RELATED);
    m.setMultipartRelatedType(IMMConstants.CT_APPLICATION_SMIL); //presentation part is written in SMIL
    m.setPresentationId("<0000>"); // presentation part has Content-ID=<0000>

  }

  private void AddContents(MMMessage m) {

    //This is where the majority of the work is done.  Note that here we are adding the parts of the
    //message in the order we want them to appear.  Actually the presentation part specifies that,
    //but in terminals which cannot understand the presentation part, the order may be significant,
    //so there seems to be no reason to use random order.

    //Note also, that the current version (1.1) of the library encodes the message in some sort
    //of random order, so developers must either fix that problem using the source code, or
    //be prepared for random order output.

    // Path where contents are stored assumed to be same as this class...
    String path = getPath();

    // Add SMIL content
    MMContent smil_part = new MMContent();
    byte[] b1 = readFile(path + "HelloWorld.smil");
    smil_part.setContent(b1, 0, b1.length);
    smil_part.setContentId("<0000>"); //If "<>" are used with this method, the result is Content-ID
    smil_part.setType(IMMConstants.CT_APPLICATION_SMIL);
    m.addContent(smil_part);

    // Add slide1 text
    MMContent s1_text = new MMContent();
    byte[] b2 = readFile(path + "HelloWorld.txt");
    s1_text.setContent(b2,0,b2.length);
    s1_text.setContentId("HelloWorld.txt"); //If "<>" are not used with this method, the result is Content-Location
    s1_text.setType(IMMConstants.CT_TEXT_PLAIN);
    m.addContent(s1_text);
    // Add slide1 image
    MMContent s1_image = new MMContent();
    byte[] b3 = readFile(path + "sample_image.jpg");
    s1_image.setContent(b3,0,b3.length);
    s1_image.setContentId("sample_image.jpg");
    s1_image.setType(IMMConstants.CT_IMAGE_JPEG);
    m.addContent(s1_image);
/*
    MMContent s1_image = new MMContent();
    byte[] b3 = readFile(path + "SmileyFace.gif");
    s1_image.setContent(b3,0,b3.length);
    s1_image.setContentId("SmileyFace.gif");
    s1_image.setType(IMMConstants.CT_IMAGE_GIF);
    m.addContent(s1_image);
*/
    // Add slide1 audio
    MMContent s1_audio = new MMContent();
    byte[] b4 = readFile(path + "HelloWorld.amr");
    s1_audio.setContent(b4,0,b4.length);
    s1_audio.setContentId("HelloWorld.amr");
    s1_audio.setType("audio/amr"); //Note how to use mime-types with no pre-defined constant!
    m.addContent(s1_audio);

    // Add slide2 text
    MMContent s2_text = new MMContent();
    byte[] b5 = readFile(path + "TheEnd.txt");
    s2_text.setContent(b5,0,b5.length);
    s2_text.setContentId("<TheEnd.txt>");  //Here, again, we are using Content-ID - just for demonstration
    s2_text.setType(IMMConstants.CT_TEXT_PLAIN);
    m.addContent(s2_text);
    // Add slide2 image
    MMContent s2_image = new MMContent();
    byte[] b6 = readFile(path + "TheEnd.gif");
    s2_image.setContent(b6,0,b6.length);
    s2_image.setContentId("<TheEnd.gif>");
    s2_image.setType(IMMConstants.CT_IMAGE_GIF);
    m.addContent(s2_image);
    // Add slide2 audio
    MMContent s2_audio = new MMContent();
    byte[] b7 = readFile(path + "YallComeBackNowYaHear.amr");
    s2_audio.setContent(b7,0,b7.length);
    s2_audio.setContentId("<YCBNYH.amr>"); //Note that the filename and Content-ID don't need to be the same
    s2_audio.setType("audio/amr");
    m.addContent(s2_audio);
  }

  private byte[] readFile(String filename) {
    int fileSize=0;
    RandomAccessFile fileH=null;

    // Opens the file for reading.
    try {
      fileH = new RandomAccessFile(filename, "r");
      fileSize = (int) fileH.length();
    } catch (IOException ioErr) {
      System.err.println("Cannot find " + filename);
      System.err.println(ioErr);
      System.exit(200);
    }

    // allocates the buffer large enough to hold entire file
    byte[] buf = new byte[fileSize];

    // reads all bytes of file
    int i=0;
    try {
       while (true) {
         try {
           buf[i++] = fileH.readByte();
         } catch (EOFException e) {
          break;
         }
       }
    } catch (IOException ioErr) {
     System.out.println("ERROR in reading of file"+filename);
    }

    return buf;
  }

  private String getPath() {
	return this.datapath;
	/*
    URL url = getClass().getResource(getClass().getName() + ".class");
    String classPath = url.getHost() + url.getFile();
    int pos = classPath.lastIndexOf("/");
    return classPath.substring(0, pos + 1);
*/
  }

  public void createMmsFile(byte[] output,String filename) {
	try	{
	  String path = getPath();
	  File f = new File( path + filename);
	  FileOutputStream out = new FileOutputStream(f);

  	  out.write(output);
	  out.close();
	}
	catch (Exception e)	{
	  System.out.println(e.getMessage());
	}
  }

  public static void main (String[] args) {
    SampleMMSCreation oa = new SampleMMSCreation();
  }

}
