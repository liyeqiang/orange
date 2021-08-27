package main

import "fmt"

type MyImplement struct{}

func (m *MyImplement) String() string {
	return "hi"
}

func GetStringer() fmt.Stringer {
	var s *MyImplement = nil

	return s
}

func main() {
	if GetStringer() == nil {
		fmt.Println("GetStringer() == nil")
	} else {
		fmt.Println("GetStringer() != nil")
	}
}
