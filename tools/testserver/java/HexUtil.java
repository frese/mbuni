public class HexUtil {

	public final static String byteToHex( byte[] bin ) {
		StringBuffer buf = new StringBuffer();

		for ( int i = 0; i < bin.length; i++ ) {
			int v = byteToInt( bin[i] );
			int hi = ( v >> 4 ) & 15;
			int low = v & 15;

			if ( hi < 10 )
				buf.append( (char) ( '0' + hi ) );
			else
				buf.append( (char) ( 'A' + ( hi - 10 ) ) );
			if ( low < 10 )
				buf.append( (char) ( '0' + low ) );
			else
				buf.append( (char) ( 'A' + ( low - 10 ) ) );
		}
		return buf.toString();
	}

	public final static int byteToInt(byte ch) {
		if ( ch >= 0 )
			return ch;
		return ch + 256;
	}

	public static byte[] hexToBytes(String hexstr)
	{
		return hexToBytes(hexstr.toCharArray());
	}

	public static byte[] hexToBytes(char[] hexstr)
	{
		byte[] bin = new byte[hexstr.length/2];
		for(int i=0; i<bin.length; i++) {
			int ch=0;
			switch(hexstr[i*2]) {
				case '1': ch += 0x10; break;
				case '2': ch += 0x20; break;
				case '3': ch += 0x30; break;
				case '4': ch += 0x40; break;
				case '5': ch += 0x50; break;
				case '6': ch += 0x60; break;
				case '7': ch += 0x70; break;
				case '8': ch += 0x80; break;
				case '9': ch += 0x90; break;
				case 'A': ch += 0xA0; break;
				case 'B': ch += 0xB0; break;
				case 'C': ch += 0xC0; break;
				case 'D': ch += 0xD0; break;
				case 'E': ch += 0xE0; break;
				case 'F': ch += 0xF0; break;
				case 'a': ch += 0xA0; break;
				case 'b': ch += 0xB0; break;
				case 'c': ch += 0xC0; break;
				case 'd': ch += 0xD0; break;
				case 'e': ch += 0xE0; break;
				case 'f': ch += 0xF0; break;
				default: break;
			}
			switch(hexstr[(i*2)+1]) {
				case '1': ch += 0x01; break;
				case '2': ch += 0x02; break;
				case '3': ch += 0x03; break;
				case '4': ch += 0x04; break;
				case '5': ch += 0x05; break;
				case '6': ch += 0x06; break;
				case '7': ch += 0x07; break;
				case '8': ch += 0x08; break;
				case '9': ch += 0x09; break;
				case 'A': ch += 0x0A; break;
				case 'B': ch += 0x0B; break;
				case 'C': ch += 0x0C; break;
				case 'D': ch += 0x0D; break;
				case 'E': ch += 0x0E; break;
				case 'F': ch += 0x0F; break;
				case 'a': ch += 0x0A; break;
				case 'b': ch += 0x0B; break;
				case 'c': ch += 0x0C; break;
				case 'd': ch += 0x0D; break;
				case 'e': ch += 0x0E; break;
				case 'f': ch += 0x0F; break;
				default: break;
			}
		}
		return bin;
	}

	public final static byte intToByte(int ch) {
		if ( ch < 0 || ch > 255 )
			throw new Error("hexUtil.intToByte - out of bounds: " + ch );
		if ( ch < 128 )
			return (byte) ch;
		return (byte) ( ch - 256 );
	}
	
	public static String hexToString(String hexstr) {
		return new String(hexToBytes(hexstr));
	}
}