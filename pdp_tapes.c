#include "pdp_tapes.h"

uint8_t tape_dpys5_d[] = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAmr+pu4CCmr+qmr+wmr+riL+wmr+smr++mr+tu4CCmr+umr+/mr+vu4CCmr+wgICAmr+xkb+wmr+yoL++mr+zlL++mr+0pL+wmr+1qr+/mr+2sL+vmr+3kL++mr+4oL+/mr+5u4CCmr+6mr++mr+7qr++mr+8voSAmr+9sL+psL+pAAAAAACagICagIW+gIC+gIC+gIC+gICwhqOchq0AAAAAAJqAiJqBgLiDh5SCrLiDv5SCrY+Bh5WCrJuCraSCrKSCraqCu7CAjLmAoJSCrpGCrJSCr5GCrZSCsLmAgaCCrJSCrLmAgaCCrZSCraiCvLCApY+BlpWCrJuCrbCAmZGCrJSCr5GCrZSCsKSCrKSCraiCvbCAsY+BlpWCrJuCrbCAqaaCrrCAvY+Bh5SDh5qDv4+Bh5SDvpqEto+Bh5SDmpqEmrCBgpCCqpSDmpCCq7GEmgAAAAAAmoGAmoKAlISasICVgoK+voiAlIKusICVgICAgICAloGGkIKqrIK/t5CBmoKqkIKrrIK/t5CBmoKroIKqlIKqtpiPsYGGgICAgICAloGVtIiAvoKAkYKtoIKwtIiAsIGgt6iBsIGjhoOAt6iBhoOBlIKwkYKsoIKvtIiAsIGqt6iBsIGthoOAt6iBhoOBlIKvkoKwlIKzmoK0t6iBlIKxkIK0t6iBlIKykIKzrIOClIK1kIK0rIOClIK2kIK1ooKylIK3voiAhoeZAAAAAACagoCag4CUgriQgrWggrKUgrm+iICUgrqQgraigrGSgrm7g4e+iIC7g4e+iICSgrq7g4e+iIC7g4eQgrGggraSgre7g4e+iIC7g4eSgri7g4e+iIC7g4eQgrS+iIC2n7+2n7+QgrO7g4e+iIC7g4eSgrS7g4e+iIC7g4eQgq+SgrCxgZWqgLOdh52AgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAgICAhLeAg7+AhLaAgLyelqOWlJwAAAAAAJqDgJqDg5+/v6+/v5utpp+zqQAAAAAAmoS5moWAgICAvpKAqoS/tYGAuLWjsYS5v7+/u5yYAAAAAACahYCahoC+koC2n7+2n7+yhoCUhp2yhoCUhp6yhoCUhp+yhoCUhqCyhoCUhqGyhoCUhqKQhpGUhpeQhpKUhpiQhpOUhpmQhpSUhpqQhpWUhpuQhpaUhpyQhpeghpiIhp2ghpqUhpqihpuIhp6+iICghpeUhpeShpq6gIeQhpiihpmIhp+ghpuUhpuihpyIhqC+iICghpiUhpiShpu6gIeQhpmihpeIhqGghpyUhpyihpqIhqK+iICghpmUhpmShpy6gIewhZuRkrgAAAAAAJqGgJqGrpaGhr6CgLaYh6CGh5aGhZCGhbCGhoCGiLeogbeog7eoh7eoj7eon7eov7epv7erv7evv72/v4CAgIKAgICAgISAgICAgICAgICAgICAgICAgICAgICAgL6EgL6EgL6EgL6EgL6EgL6EgL6SgKqGrbWBgLiTpaCAh5SAh7afv4aAh7qDh7CGo7+/v5KyuAAAAAAAsICIAA==";
tape_t tape_snowflake = {
	"Snowflake",
	100,
	sizeof(tape_dpys5_d),
	tape_dpys5_d
};