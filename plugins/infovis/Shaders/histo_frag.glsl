uniform int selected = 0;

void main(void) {
    if (selected == 1) {
        gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
    } else {
        gl_FragColor = vec4(0.0, 1.0, 0.0, 1.0);
    }
}
