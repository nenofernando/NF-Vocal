# NF Vocal

Processador vocal da NF Audio Tools para macOS, nos formatos VST3 e Audio Unit (AU).
Interface preta com logo NF, oito faders, seis modulos com tres modos cada,
medidores, presets, comparacao A/B e salvamento de configuracoes.

## Correcao do menu de presets

Revisao de texto da versao 1.0.1: os separadores dos presets usam ASCII
(`01 - Clean / Reset`, `02 - Pop Lead` etc.). Isso evita caracteres corrompidos
causados pela interpretacao de um literal UTF-8 como texto de outra codificacao.
Esta revisao mantem os identificadores de parametros e o formato de estado.

A versao 1.0.1 tambem corrige o reset dos parametros ao reabrir o painel,
limita as alcas dos faders, centraliza o titulo, reduz o tamanho inicial para
800 x 488 e otimiza o processamento de audio e o redesenho dos medidores.

## Compilar no macOS

Requisitos: ferramentas de desenvolvimento do Xcode, CMake 3.22+ e JUCE 8.0.12.
O JUCE e uma dependencia externa: nao esta incluido neste repositorio.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DJUCE_SOURCE_DIR=/caminho/para/JUCE \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build --config Release --parallel 3
```

Tambem e possivel colocar uma copia do JUCE em `ThirdParty/JUCE`, que e o
caminho padrao. O logo em `Assets/Logo.png` e incorporado ao binario.

Os plugins compilados ficam em:

- `build/NFVocal_artefacts/Release/VST3/NF Vocal.vst3`
- `build/NFVocal_artefacts/Release/AU/NF Vocal.component`

## Testes

```sh
./build/NFVocalCheck
./build/NFVocalCheck --reopen
./build/NFVocalCheck --benchmark
```

O teste principal verifica audio mono/estereo em 44,1/48/96 kHz, modos dos
efeitos, blocos variaveis, recuperacao de estado, reabertura da janela,
limites dos faders, redimensionamento e caracteres ASCII nos presets.
O teste de reabertura processa eventos reais da interface para verificar
que abrir a janela nao reinicializa os parametros.

## Instalar manualmente

Feche a DAW. Copie `.vst3` para `/Library/Audio/Plug-Ins/VST3/` e
`.component` para `/Library/Audio/Plug-Ins/Components/`, ou use as pastas
equivalentes em `~/Library/Audio/Plug-Ins/` para instalar apenas no usuario.
Reabra a DAW e atualize a lista de plugins. Substitua a copia anterior em vez
de manter instalacoes duplicadas. Consulte `LEIA-ME.txt` para os controles.

## Escopo e dependencias

O DSP e original, inspirado nas funcoes da referencia fornecida. Nao contem
codigo ou recursos graficos da Waves e nao foi validado como reproducao
sonora exata do CLA Vocals. PITCH e microafinacao/duplicacao, nao afinacao automatica.

O logo foi fornecido pelo titular do projeto. Consulte `JUCE-LICENSE.md`
antes de redistribuir produtos compilados com JUCE. Este repositorio nao
concede uma licenca adicional sobre o logo ou sobre dependencias externas.
Os instaladores locais usam assinatura ad hoc, sem notarizacao Apple.

`VALIDACAO.txt` registra os testes locais e suas limitacoes. A comparacao
de CPU ali descrita compara o DSP 1.0 com o 1.0.1, nao esta revisao de texto.
