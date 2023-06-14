[BITS 16]
[ORG 0x7C00]

_start:
    ; Инициализация сегментов данных и стека
    mov ax, cs
    mov ds, ax
    mov ss, ax
    mov sp, _start ; Начало стека - адрес первой команды

    ;чтение ядра
	mov ax, 0x1000
	mov es, ax ;размер буфера
	mov bx, 0x00 ;адрес буффера(0x0000: 0x1000)
	mov ah, 0x02 ;режим чтения

	mov dl, 1 ;номер диска
	mov dh, 0x00 ;номер головки
	mov cl, 0x01 ;номер сектора
	mov ch, 0x00 ;номер цилиндра
	mov al, 35 ;кол-во секторов. 
	int 0x13

	xor ch, ch ; Номер выбранного пункта меню (начинаем с первого)
    call menu ; Прыгаем на меню
    
    menu:
        ; Чистка всего экрана
        mov ax, 3
        int 0x10

        xor dh, dh ; Счетчик цикла
        mov di, colors ; Указатель на массив цветов
        mov si, menu_items ; Указатель на указатели строк
        mov ah, 0x09 ; Вывод символов
        mov bl, 0x07 ; Цвет символов (белый на черном фоне)
        xor ah, ah

        menu_loop:
            cmp ch, dh ; Сравниваем
            je print_selected ; Если сейчас будет печататься выбранный пункт меню, то прыгаем
            jmp print_char ; Иначе просто продолжаем печатать символы с обычным цветом
     
    find_color:
    		inc ah ; Инкрементируем индекс
    		inc di ; Инкрементируем указатель
    		jmp print_selected
    		  	
    print_selected:
    		cmp ah, ch ; Ищем индекс цвета в массиве colors в соответствии с выбранным пунктом
    		jne find_color
            mov bl, byte [di] ; Ставим на bl байт цвета текста
            mov cl, bl
            jmp print_char
        
    print_char:
    	mov al, byte [si] ; Выбираем символ из массива
    	pusha ; Сохраняем на стек значения регистров
    	mov cx, 1 ; Печатаем символ один раз
        mov ah, 0x09 ; Принт символа с цветом
        int 0x10
        popa ; Возврат значения регистров
        
        ; Блок перемещения курсора для корректной работы функции 0x09
        pusha
        mov bh, 0
        mov ah, 0x03
        int 0x10
        inc dl
        mov ah, 0x02
        int 0x10
        popa
        
        inc si ; Бегаем по массиву строк
        cmp byte [si], 0x00 ; Пока это не конец строки - печатаем ее
        jne print_char
		inc si ; Убираем указатель с 0x00 байта
        
        ; Блок перемещения курсора для корректной работы функции 0x09
        pusha   
        mov bh, 0
        mov ah, 0x03
        int 0x10   
        inc dh
        mov dl, 0
        mov ah, 0x02
        int 0x10
        popa

        cmp dh, 5 ; Если напечатали последнюю строку - ждем взаимодействия пользователя
        je wait_for_input ; Взаимодействие пользователя
        inc dh ; Print string
        mov bl, 0x07 ; Восстанавливаем цвет на всякий случай
        jmp menu_loop

    wait_for_input:
        ; Ожидание ввода пользователя и обработка нажатий клавиш "вверх" и "вниз"
        mov ah, 0x00 ; Функция чтения клавиши BIOS
        int 0x16 ; Вызов прерывания BIOS для чтения клавиши
        cmp ah, 0x48 ; Проверка на нажатие клавиши "вверх"
        je move_up ; Если это клавиша "вверх", то переходим к предыдущему пункту меню
        cmp ah, 0x50 ; Проверка на нажатие клавиши "вниз"
        je move_down ; Если это клавиша "вниз", то переходим к следующему пункту меню
        cmp ah, 0x1C
        je load_kernel
        jmp wait_for_input ; Если это другая клавиша, то ожидаем новый ввод

        move_up:
            cmp ch, 0 ; Проверка на начало списка пунктов меню
            je wait_for_input ; Если это начало списка, то ожидаем новый ввод
            dec ch ; Переходим к предыдущему пункту меню
            jmp menu ; Выводим меню заново

        move_down:
            cmp ch, 5 ; Проверка на конец списка пунктов меню
            je wait_for_input ; Если это конец списка, то ожидаем новый ввод
            inc ch ; Переходим к следующему пункту меню
            jmp menu ; Выводим меню заново


colors:
    db 0x07, 0x0F, 0x0E, 0x03, 0x05, 0x02

menu_items:
    db "Gray", 0
    db "White", 0
    db "Yellow", 0
    db "Cyan", 0
    db "Magenta", 0
	db "Green", 0


load_kernel:
    xor bh, bh
    mov bl, cl ; цвет текста	
	mov ax, 3
    int 0x10

    cli
    lgdt [gdt_info]
    in al, 0x92
    or al, 2
    out 0x92, al
    mov eax, cr0
    or al, 1
    mov cr0, eax
    jmp 0x8:protected_mode

gdt:
    db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    db 0xff, 0xff, 0x00, 0x00, 0x00, 0x9A, 0xCF, 0x00
    db 0xff, 0xff, 0x00, 0x00, 0x00, 0x92, 0xCF, 0x00

gdt_info:
    dw gdt_info - gdt
    dw gdt, 0

; Метка для перехода в защищенный режим
[BITS 32]
protected_mode:
    mov ax, 0x10    
    mov es, ax
    mov ds, ax
    mov ss, ax

    call 0x10000

    times (512 -($ - _start)- 2) db 0
    db 0x55, 0xAA
