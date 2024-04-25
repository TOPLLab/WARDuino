import {print} from "as-warduino/assembly";

export function main(): void {
    print("ASCII Table ~ Character Map\n");
    let byte: i32 = 33;

    while (byte !== 126) {
        print(String.fromCharCode(byte));

        print(", dec: " + byte.toString());
        print(", hex: " + byte.toString(16));
        print(", oct: " + byte.toString(8));
        print(", bin: " + byte.toString(2) + "\n");

        if (byte == 126) {
          while (true) {
            continue;
          }
        }

        byte++;
    }
}
