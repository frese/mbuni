public class HexToString {
	public static void main(String args[]) {
		System.out.println("Hex: "+ args[0]);
		byte[] bytes = HexUtil.hexToBytes(args[0]);
		System.out.println("bytes length = " + bytes.length);
		
		String nohex = new String(bytes);
		System.out.println("As str: [" + nohex + "]");
	}
}