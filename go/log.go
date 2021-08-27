package main

type LogWriter interface {
	Write(data interface{}) error
}

type Logger struct {
	writerList []LogWriter
}

func (l *Logger) RegisterWriter(writer LogWriter) {
	l.writerList = append(l.writeList, writer)
}

func (l *Logger) Log(data interface{}) {
	for _, writer := range l.writerList {
		writer.Write(data)
	}
}

func NewLogger() *Logger {
	return &Logger{}
}
