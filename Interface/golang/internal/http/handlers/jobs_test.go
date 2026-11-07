package handlers

import (
	"testing"

	"github.com/gin-gonic/gin/binding"
)

func TestCreateJobDTOTimeoutSec(t *testing.T) {
	cases := []struct {
		name    string
		body    string
		wantErr bool
		want    *int
	}{
		{name: "absent", body: `{"task_name":"wordcount"}`},
		{name: "null", body: `{"task_name":"wordcount","timeout_sec":null}`},
		{name: "in range", body: `{"task_name":"wordcount","timeout_sec":30}`, want: intPtr(30)},
		{name: "zero", body: `{"task_name":"wordcount","timeout_sec":0}`, wantErr: true},
		{name: "above max", body: `{"task_name":"wordcount","timeout_sec":3601}`, wantErr: true},
	}

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			var dto createJobDTO
			err := binding.JSON.BindBody([]byte(tc.body), &dto)
			if tc.wantErr {
				if err == nil {
					t.Fatalf("expected binding error for %s", tc.body)
				}
				return
			}
			if err != nil {
				t.Fatalf("unexpected binding error: %v", err)
			}
			if (dto.TimeoutSec == nil) != (tc.want == nil) || (tc.want != nil && *dto.TimeoutSec != *tc.want) {
				t.Fatalf("TimeoutSec = %v, want %v", dto.TimeoutSec, tc.want)
			}
		})
	}
}

func intPtr(v int) *int { return &v }
