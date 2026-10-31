# NF Vocal 1.2.0

Processador vocal NF Audio Tools para macOS, AU e VST3 (Intel e Apple Silicon).

## Módulos móveis

Arraste a alça no nome de um módulo e solte à esquerda ou à direita de outro
para colocá-lo antes ou depois. Funciona entre as duas fileiras, inclusive
para colocar Gate ou De-esser depois de Pitch. Também é possível clicar com
o botão direito no nome e escolher “Mover antes de” ou “Mover depois de”.
A numeração mostra a ordem real: primeira fileira da esquerda para a direita,
seguida pela segunda. Input e Output permanecem nas extremidades do áudio.
A sequência é salva no preset, no projeto da DAW e nas memórias A/B.

Todos os efeitos passam a formar uma cadeia em série, com suas misturas
internas. Mudar a posição altera o áudio e o sinal recebido pelos efeitos
seguintes. A combinação Reverb/Delay/Pitch pode soar diferente da versão
1.1, que somava os três efeitos em paralelo a partir do mesmo sinal.
As identificações dos parâmetros anteriores são preservadas.

## Controles

- BASS, MID, TREBLE, COMPRESS, REVERB, DELAY e PITCH: faders com modo e ON.
- MID: -12 a +12 dB; modos BODY (400 Hz), FOCUS (1,2 kHz) e PRESENCE (2,6 kHz).
- DE-ESSER: redução dinâmica de sibilância; zero desliga.
- GATE: expansão descendente progressiva; zero desliga.
- SATURATOR: drive mais agressivo, até 24 vezes, com mistura de recorte suave
  e rígido nos valores altos. Zero desliga.
- FLANGER: modulação estéreo; zero desliga.
- LOW-CUT: passa-altas de 20 a 600 Hz; 20 Hz desliga.
- HIGH-CUT: passa-baixas de 2 a 20 kHz; 20 kHz desliga.

Acabamento dourado brilhante nos faders, knobs, aros, marca e assinatura.
Logo à esquerda e título centralizado.
Medidor horizontal de redução de ganho com escala de 0 a 24 dB e valor numérico.
Duplo clique restaura o valor padrão do controle. Duplo clique no logo
restaura a janela para 800 x 614. PITCH é duplicação com microafinação;
não é correção automática de afinação. Presets de fábrica restauram também
a sequência original. Presets antigos recebem o roteamento padrão e novos
controles em valores neutros.

## Compilar

Requisitos: macOS, ferramentas Xcode, CMake 3.22+ e JUCE 8.0.12 externo.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 -DJUCE_SOURCE_DIR=/caminho/JUCE '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build --config Release --parallel 3
./build/NFVocalCheck
./build/NFVocalCheck --reopen
```

Um caminho absoluto PNG como argumento do teste salva uma captura real.
Os testes verificam também drag/drop, sequência real no DSP, persistência,
A/B, efeitos, harmônicos de saturação, áudio mono/estéreo e blocos variáveis.

## Instalar

Feche a DAW. Copie `NF Vocal.vst3` para VST3 e `NF Vocal.component` para
Components em `~/Library/Audio/Plug-Ins/`. Substitua a cópia anterior e
atualize a lista da DAW. Binários com assinatura ad hoc, sem notarização.
Consulte JUCE-LICENSE.md antes de redistribuir.

Revisão visual compacta: knobs inferiores menores, aros escuros em repouso
e arco dourado somente durante o arraste. Textos de orientação e subtítulo
foram removidos do painel. A ajuda de operação permanece neste arquivo.
