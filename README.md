# AUTORAMA

Este projeto consiste na modelagem e no controle da velocidade de um veículo em escala acionado por motor DC, utilizando uma pista instrumentada com sensores e comunicação entre módulos embarcados.

Inicialmente, é realizada a análise do sistema em malha aberta, na qual são aplicados diferentes valores de PWM ao motor e medida a velocidade do veículo ao longo do tempo. A partir desses dados, são obtidas a relação entre PWM e velocidade em regime permanente e a resposta dinâmica do sistema, permitindo a identificação de um modelo matemático aproximado.

A lógica completa, é esta:
- O operador usa o controle remoto para enviar comandos à pista e ao carro. 
- A pista recebe esses comandos no Arduino 1, que coordena o sistema, aciona a catraca, recebe por rádio a velocidade do carro e lê parte dos sensores LDR. 
- O Arduino 2 complementa a leitura dos demais sensores e registra os dados no cartão SD. 
- O carro, por sua vez, mede sua própria velocidade com o sensor Hall, envia essa informação continuamente para a pista e recebe dela os comandos de PWM que determinam a atuação do motor. 
- Paralelamente, a pista mede a passagem do carro por sensores LDR distribuídos ao longo do trajeto para validar posição e velocidade. 
- No modo de controle, a pista compara a velocidade desejada com a velocidade medida, calcula o erro, aplica a lei de controle e envia ao carro um novo PWM, fechando a malha.

## Codigos
Codigos dos Arduinos:

- Carro: Arduino_Carro.ino
- Controle: Controle_Remoto.ino 
- Pista: Arduino_1-PISTA_DO_AUTORAMA.ino e Arduino_2-PISTA_DO_AUTORAMA.ino

## Módulos

1.  Carro (Módulo Embarcado)

    Microcontrolador: Arduino Nano

    Comunicação Sem Fio: Módulo RF nRF24L01

    Atuador de Tração: Motor DC com caixa de redução (12:24)

    Driver de Potência: Ponte H L293D

    Sensor de Velocidade: Sensor Hall LM393 (leitura de ímanes na roda)

    Sensor de Distância: VL53L0X (Time of Flight - Laser)

    Regulação de Tensão Primária: Conversor Step-Down MP1584 (15V para 7V)

    Regulação de Tensão Lógica: AMS1117 (5V e 3.3V)

    Interface Visual: LEDs (Verde, Azul, Vermelho)

    Monitorização: Voltímetro digital embarcado

    Filtragem: Capacitores eletrolíticos e cerâmicos

2. Pista (Sistema Central de Processamento)

    Processamento de Controle: Arduino Uno (Mestre)

    Processamento de Dados/Log: Arduino Uno (Escravo)

    Comunicação Interna: Conexão Serial (SoftwareSerial)

    Comunicação com Carro/Controle: Módulo RF nRF24L01

    Armazenamento: Módulo Leitor de Cartão MicroSD

    Sensores de Posicionamento: 8 Sensores LDR (3 no Arduino de controle + 5 no de aquisição)

    Emissores para LDR: Diodos Laser

    Atuador de Bloqueio: Servo Motor (Catraca)

    Interface Visual: LEDs de status (Amarelo, Verde, Vermelho, Azul)

    Fonte de Alimentação: Fonte chaveada 24V

    Regulação de Potência: Módulos LM2596 (para trilhos e eletrónica)

3. Controle Remoto (Interface do Usuário)
    Microcontrolador: Arduino Uno

    Comunicação: Módulo RF nRF24L01

    Entrada de Dados: Teclado Matricial 4x4

    Saída de Dados: Display OLED 0.91”

    Comandos Diretos: Botões de pressão (Enter, Reset, Finalizar, Modo)

    Regulação: AMS1117 3.3V (específico para o rádio)
