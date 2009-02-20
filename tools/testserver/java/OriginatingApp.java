/*
	* @(#)OriginatingApp.java	1.1
	*
	* Copyright (c) Nokia Corporation 2002
	*
	*/
import java.io.*;
import java.util.*;
import java.net.*;
import com.nokia.mms.*;

public class OriginatingApp {
	private static String datapath = "../data/";
	private static String sendurl = "http://127.0.0.1:8189";
	private static String username = "user";
	private static String password = "pass";

	public OriginatingApp() {
		MMMessage mm = new MMMessage();
		SetMessage(mm);
		AddContents(mm);

		MMEncoder encoder=new MMEncoder();
		encoder.setMessage(mm);

		try {
			encoder.encodeMessage();
			byte[] out = encoder.getMessage();
			
			MMSender sender = new MMSender();
			sender.setMMSCURL(sendurl);
			//sender.addHeader("X-NOKIA-MMSC-Charging", "100");
			sender.addHeader("Authorization", create_auth_string(username,password));

			MMResponse mmResponse = sender.send(out);
			System.out.println("Message sent to " + sender.getMMSCURL());
			System.out.println("Response code: " + mmResponse.getResponseCode() + " " + mmResponse.getResponseMessage());
			
			Enumeration keys = mmResponse.getHeadersList();
			while (keys.hasMoreElements()){
				String key = (String) keys.nextElement();
				String value = (String) mmResponse.getHeaderValue(key);
				System.out.println(key + ": " + value);
			}

		} catch (Exception e) {
			System.out.println(e.getMessage());
		}
	}
	
	public String create_auth_string(String user, String pass) {
		return "Basic " + Base64Coder.encodeString(user + ":" + pass);
	}

	private void SetMessage(MMMessage mm) {
		mm.setVersion(IMMConstants.MMS_VERSION_10);
		mm.setMessageType(IMMConstants.MESSAGE_TYPE_M_SEND_REQ);
		mm.setTransactionId("0000000066");
		mm.setDate(new Date(System.currentTimeMillis()));
		mm.setFrom("+358990000066/TYPE=PLMN");
		mm.addToAddress("+358990000005/TYPE=PLMN");
		mm.addToAddress("123.124.125.125/TYPE=IPv4");
		mm.addToAddress("1234:5678:90AB:CDEF:FEDC:BA09:8765:4321/TYPE=IPv6");
		mm.addToAddress("john.doe@nokia.com");
		mm.setDeliveryReport(true);
		mm.setReadReply(false);
		mm.setSenderVisibility(IMMConstants.SENDER_VISIBILITY_SHOW);
		mm.setSubject("This is a nice message!!");
		mm.setMessageClass(IMMConstants.MESSAGE_CLASS_PERSONAL);
		mm.setPriority(IMMConstants.PRIORITY_LOW);
		mm.setContentType(IMMConstants.CT_APPLICATION_MULTIPART_MIXED);
		//    In case of multipart related message and a smil presentation available
		//    mm.setContentType(IMMConstants.CT_APPLICATION_MULTIPART_RELATED);
		//    mm.setMultipartRelatedType(IMMConstants.CT_APPLICATION_SMIL);
		//    mm.setPresentationId("<A0>"); // where <A0> is the id of the content containing the SMIL presentation
	}

	private void AddContents(MMMessage mm) {
		/*Path where contents are stored*/
		//String path = getPath();

		// Adds text content
		MMContent part1 = new MMContent();
		byte[] buf1 = readFile(datapath + "sample_text.txt");
		part1.setContent(buf1, 0, buf1.length);
		part1.setContentId("<0>");
		part1.setType(IMMConstants.CT_TEXT_PLAIN);
		mm.addContent(part1);

		// Adds image content
		MMContent part2 = new MMContent();
		byte[] buf2 = readFile(datapath + "sample_image.jpg");
		part2.setContent(buf2, 0, buf2.length);
		part1.setContentId("<1>");
		part2.setType(IMMConstants.CT_IMAGE_JPEG);
		mm.addContent(part2);
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
		URL url = getClass().getResource(getClass().getName() + ".class");
		String classPath = url.getHost() + url.getFile();
		int pos = classPath.lastIndexOf("/");
		return classPath.substring(0, pos + 1);
	}

	public static void print_help() {
			System.out.println("The program expects the sendurl as it's first argument.");
	}

	public static void main (String[] args) {
		//System.out.println("foo =  "+ args.length);
		if (args.length >= 1) {
			sendurl = args[0];
		} else {
			print_help();
			return;
		}
		OriginatingApp oa = new OriginatingApp();
	}
}