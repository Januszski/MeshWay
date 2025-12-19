// package main
//
// import (
// 	"fmt"
// 	"log"
//
// 	"go.bug.st/serial"
// )
//
// func main() {
// 	portName := "/dev/ttyUSB1" // CHANGE THIS if needed
// 	baudRate := 115200
//
// 	mode := &serial.Mode{
// 		BaudRate: baudRate,
// 	}
//
// 	port, err := serial.Open(portName, mode)
// 	if err != nil {
// 		log.Fatal(err)
// 	}
// 	defer port.Close()
//
// 	fmt.Println("Connected to", portName)
//
// 	buf := make([]byte, 256)
//
// 	for {
// 		n, err := port.Read(buf)
// 		if err != nil {
// 			log.Fatal(err)
// 		}
//
// 		if n > 0 {
// 			// Print raw bytes as hex
// 			for i := 0; i < n; i++ {
// 				fmt.Printf("%02X ", buf[i])
// 			}
// 			fmt.Println()
//
// 			// ALSO print as ASCII (like Python often does)
// 			fmt.Println(string(buf[:n]))
// 		}
// 	}
// }
