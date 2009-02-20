import java.io.*;
import java.util.*;
import java.net.*;
import com.nokia.mms.*;

public class DecodeMessage {
	public static void main(String args[]) {
		String message_hex;
		
		if (args.length == 1) {
			message_hex = args[0];
		} else {
			System.out.println("Requires a message string (application/vnd.wap.mms-message) in hexadecimal format");
			return;
		}
		
		byte[] message_binary = HexUtil.hexToBytes(message_hex);
		
		MMDecoder decoder = new MMDecoder();
		decoder.setMessage(message_binary);
		try {
			decoder.decodeMessage();
			System.out.println("Successfully decoded message...");
		} catch (MMDecoderException e) {
			System.out.println(e.toString());
		}
		
		MMMessage msg = decoder.getMessage();
		
		System.out.println("Message ID: " + msg.getMessageId());
		System.out.println("Transaction ID: " + msg.getTransactionId());
		System.out.println("Version: " + msg.getVersionAsString());
		System.out.println("Subject: " + msg.getSubject());
		System.out.println("Content-Type: "+ msg.getContentType());
		System.out.println("From: " + msg.getFrom().getFullAddress());
		
	}
}