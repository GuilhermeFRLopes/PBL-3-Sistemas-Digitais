void* read_mouse(void* arg) {
    
    ssize_t n;
    int x, y;

    while (1) {
        n = read(fd_mouse, &ev, sizeof(ev));
        if (n == (ssize_t)-1) {
            perror("Error reading");
            continue;
        } else if (n != sizeof(ev)) {
            fprintf(stderr, "Error: read %ld bytes, expecting %ld\n", n, sizeof(ev));
            continue;
        }

        pthread_mutex_lock(&lock);

        if (ev.type == EV_REL && ev.code == REL_X) {
            // Detecta direção de movimento
            if (ev.value > 0) {
                // Movimento para a direita
                printf("Movendo para a direita: %d\n", ev.value);
            } else if (ev.value < 0) {
                // Movimento para a esquerda
                printf("Movendo para a esquerda: %d\n", ev.value);
            }

            // Acumula o valor de movimento
            x_mouse += ev.value;
            printf("Posição acumulada X: %d\n", x_mouse);
        }

        if (ev.type == EV_REL && ev.code == REL_Y) {
            y_mouse += ev.value;
            printf("Posição acumulada Y: %d\n", y_mouse);
        }

        // Limitar as coordenadas para evitar overflow
        if (x_mouse < 0) x_mouse = 0;
        if (x_mouse > 619) x_mouse = 619;

        pthread_mutex_unlock(&lock);
    }

    return NULL;
}
