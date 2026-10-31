# Integração NF Vocal 1.2.0 na Mixa

O processador continua sendo `NFVocal` (subclasse de `juce::AudioProcessor`)
em `Source/Plugin.h`. A fábrica padrão continua retornando esse processador.
Nome, bundle ID, fabricante e código VST3/AU foram mantidos.

## Recursos necessários

- Compilar `Source/Plugin.cpp` e `Source/Plugin.h` com JUCE 8.0.12.
- Incorporar **os dois** arquivos `Assets/Logo.png` e `Assets/Signature.png`
  ao BinaryData consumido pelo código. O alvo `NFAssets` do CMake já faz isso.
- Vincular `juce_audio_utils` e `juce_dsp`, além de `NFAssets`, como no CMake.
- Se o aplicativo hospedeiro já define sua própria `createPluginFilter`,
  compilar o código NF Vocal com `NFVOCAL_NO_PLUGIN_FACTORY=1` e instanciar
  `NFVocal` diretamente. Esse define só remove a fábrica global; preserva
  o processador, o editor e os parâmetros.
- O JUCE é uma dependência externa; não foi adicionado ao repositório.
- O executável de testes `NFVocalCheck` usa o loop de eventos do macOS.
  Ele não é necessário para incorporar o processador ao aplicativo nativo.

## Estado e roteamento

Os identificadores antigos foram mantidos. `midMode` e `midOn` foram
acrescentados ao final da lista de parâmetros da versão 1.1, preservando
sua ordem anterior. `mid` continua sendo o parâmetro de ganho já existente;
agora seu controle visual é um fader. Não é um recurso MIDI.

A ordem dos treze efeitos é salva como propriedade `chainOrder` no estado
XML de `getStateInformation`, junto com os parâmetros. O aplicativo deve
salvar e restaurar esse estado completo. Salvar somente os valores numéricos
perderia a ordem dos módulos. A/B e presets próprios já usam o estado completo.

`chainOrder()` retorna uma permutação de `NFVocal::Module`; `moveModule(id,
indiceFinal)` move um efeito e notifica o host de uma alteração de estado.
`setChainOrder(order)` rejeita valores repetidos ou fora do intervalo.
O processamento lê uma sequência atômica por bloco, sem bloqueios ou
alocações de memória no caminho de áudio. O roteamento é em série, na ordem
numerada da interface: fileira de cima e depois a de baixo. Input e Output
continuam fixos no início e no fim.

## Diferenças de áudio

O Saturator ficou mais agressivo (drive até 24 vezes e componente de recorte
rígido na parte alta do controle). A combinação Reverb/Delay/Pitch agora
funciona em série; na implementação anterior, esses efeitos eram somados
em paralelo a partir do mesmo sinal. Presets antigos continuam carregando,
mas combinações desses efeitos podem soar diferentes. Essa mudança permite
que um Gate ou De-esser colocado após Pitch realmente receba sua saída.

## Compilar os plugins para os instaladores macOS

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
 -DJUCE_SOURCE_DIR=/caminho/JUCE '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build --config Release --parallel 3
./build/NFVocalCheck
./build/NFVocalCheck --reopen
```

Artefatos:

- `build/NFVocal_artefacts/Release/AU/NF Vocal.component`
- `build/NFVocal_artefacts/Release/VST3/NF Vocal.vst3`

A captura em `Docs/NF-Vocal-1.2.0.png` é gerada pelo editor compilado.
Os binários locais possuem assinatura ad hoc. Os instaladores de distribuição
devem usar o fluxo de assinatura e notarização da Mixa. A assinatura dourada
na interface é a assinatura visual do autor e não substitui assinatura Apple.
