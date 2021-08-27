package main

import "fmt"
import "errors"
import "os"

type fileWrite struct {
	file *os.File
}

func (f *fileWriter) SetFile(filename string) (err error) {
	if f.file != nil {
		f.file.Close()
	}

	f.file, err = os.Create(filename)

	return err
}

func (f *fileWriter) Write(data interface{}) error {
	if f.file == nil {
		return errors.New("file not created")
	}

	str := fmt.Sprintf("%v\n", data)

	_, err := f.file.Write([]byte(str))

	return err
}

func newFileWriter() *fileWriter {
	return &fileWriter{}
}
