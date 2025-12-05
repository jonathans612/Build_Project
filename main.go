package main

import (
	"fmt"

	"atomicgo.dev/keyboard"
	"atomicgo.dev/keyboard/keys"
	mqtt "github.com/eclipse/paho.mqtt.golang"
)

type Input = byte

const (
	UP    Input = 'U'
	DOWN  Input = 'D'
	LEFT  Input = 'L'
	RIGHT Input = 'R'
	QUIT  Input = 'q'
)

func publish(input Input, client mqtt.Client) {
	token := client.Publish("web/initbuild2025/boat/movement", 0, false, []byte{input})
	token.Wait()
}

func processKeystroke(key keys.Key, inputBuffer chan byte) (stop bool, err error) {
	switch key.Code {

	case keys.RuneKey:
		if key.String() == "q" {
			inputBuffer <- QUIT
			return true, nil
		}
		if key.String() == "c" {
			inputBuffer <- 'C'
		}
	case keys.Up:
		inputBuffer <- UP
	case keys.Down:
		inputBuffer <- DOWN
	case keys.Left:
		inputBuffer <- LEFT
	case keys.Right:
		inputBuffer <- RIGHT
	}

	return false, nil
}

func processInput(inputBuffer chan byte, client mqtt.Client) {
	for {
		input := <-inputBuffer

		if input == QUIT {
			return
		}

		publish(input, client)
	}
}

const BROKER_DNS = "test.mosquitto.org"
const PORT = 1883

func main() {

	opts := mqtt.NewClientOptions()

	opts.AddBroker(fmt.Sprintf("tcp://%s:%d", BROKER_DNS, PORT))
	opts.SetClientID("boat_controller")
	opts.OnConnect = func(client mqtt.Client) {
		fmt.Println("Connected...")
	}

	client := mqtt.NewClient(opts)

	if token := client.Connect(); token.Wait() && token.Error() != nil {
		panic(token.Error())
	}

	inputBuffer := make(chan byte, 30)

	go processInput(inputBuffer, client)

	fmt.Println("Taking input...")
	keyboard.Listen(func(key keys.Key) (stop bool, err error) {
		return processKeystroke(key, inputBuffer)
	})

	client.Disconnect(250)
	fmt.Println("Exiting keystroke listener")
}
