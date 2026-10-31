# 1.2.0

- Acabamento dourado brilhante em faders, knobs, aros, detalhes de botões,
  marca e assinatura; fundo escuro.
- NF Audio Tools na ponta esquerda do cabeçalho e NF Vocal centralizado.
- Mid convertido em fader entre Bass e Treble, com modos BODY/FOCUS/PRESENCE
  e ON; identificador `mid` preservado.
- De-esser, Gate, Saturator, Flanger, Low-cut e High-cut como knobs.
- Saturator mais agressivo, com drive de até 24 vezes e componente de
  recorte rígido nos valores altos.
- Treze módulos móveis por alça no título ou menu contextual “antes/depois”.
  A ordem numerada controla o processamento em série e aceita movimentos
  entre as duas fileiras. Input e Output ficam fixos.
- Ordem salva no estado, presets e comparação A/B, com validação de sequência.
- Medidor horizontal de compressão (0 a 24 dB) ligado à redução real do
  compressor, com leitura numérica.
- Medidor de saída reposicionado para não sobrepor divisórias ou valores.
- Documentação para integrar o processador nativo à Mixa e opção
  `NFVOCAL_NO_PLUGIN_FACTORY` para evitar fábrica global duplicada.

A combinação Reverb/Delay/Pitch passa de soma paralela a processamento em
série. Os parâmetros anteriores permanecem identificados, mas presets com
esses efeitos combinados podem soar diferentes. Consulte INTEGRACAO-MIXA.md.
