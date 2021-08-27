package main

type LogWriter interface {
	Write(data interface{}) error
}

type Logger struct {
	writerList []LogWriter
}


func (l *Logger)
