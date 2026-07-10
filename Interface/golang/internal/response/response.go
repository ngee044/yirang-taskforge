// Package response defines the shared API envelope {success, data, error}.
package response

import "github.com/gin-gonic/gin"

// ErrorBody is the error payload of the envelope.
type ErrorBody struct {
	Code    string `json:"code"`
	Message string `json:"message"`
}

// Envelope is the common response wrapper of every interface implementation.
type Envelope struct {
	Success bool       `json:"success"`
	Data    any        `json:"data,omitempty"`
	Error   *ErrorBody `json:"error,omitempty"`
}

// Success writes a successful envelope with the given HTTP status.
func Success(c *gin.Context, status int, data any) {
	c.JSON(status, Envelope{Success: true, Data: data})
}

// Error writes a failed envelope with the given HTTP status.
func Error(c *gin.Context, status int, code, message string) {
	c.JSON(status, Envelope{Success: false, Error: &ErrorBody{Code: code, Message: message}})
}
