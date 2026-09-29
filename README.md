# Classic Mix

Classic Mix é um aplicativo macOS focado em uma tarefa: importar stems, analisá-los e construir uma mixagem/masterização assistida por IA.

## Direção do produto

- Importação de WAV, AIFF, FLAC e MP3.
- Análise local das faixas: duração, sample rate, pico, RMS, LUFS estimado, crest factor, silêncio, clipping, energia por faixa de frequência e panorama.
- Mix não destrutiva: os arquivos originais nunca são alterados.
- A IA recebe somente métricas e características extraídas localmente.
- A IA devolve um plano estruturado de mix; o motor local aplica ganho, panorama, filtros, EQ, compressão, reverb e limiter.
- Masterização em uma etapa separada, com alvo configurável de loudness e true-peak ceiling.
- Exportação WAV e relatório do que foi alterado.

## Escopo inicial

Esta primeira base não é uma DAW tradicional. Não terá timeline MIDI, piano roll, routing complexo, dezenas de menus ou recursos de composição. A tela principal será uma sessão de mix com uma lista de stems, indicadores de análise, plano da IA e controles de aprovação.

O primeiro renderizador gera uma mix estéreo nova, aplica ganho e panorama por stem e faz uma etapa inicial de masterização com alvo aproximado de loudness e teto de pico. O render é não destrutivo; o arquivo de saída é criado separadamente.

## Segurança do áudio

O processamento é offline e reversível. Nenhuma alteração é escrita nos arquivos importados. Cada render gera um novo arquivo e salva o plano usado para que a mix possa ser repetida.

## Usar o GPT

1. Abra o aplicativo e clique em **Configurar GPT**.
2. Cole sua chave da API da OpenAI no campo exibido e salve. A chave fica somente no Mac, em `~/Library/Application Support/Classic Mix/gpt-api-key.txt`, e não é enviada para o GitHub.
3. Importe os stems e clique em **Analisar e criar mix**. O aplicativo extrai as métricas localmente e envia apenas essas métricas ao GPT; os arquivos de áudio não saem do computador.
4. Revise o plano mostrado e clique em **Renderizar WAV**.

Se nenhuma chave for configurada, o aplicativo continua funcionando com um plano local de fallback.

## Build no macOS

```sh
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

O projeto usa JUCE diretamente como camada de áudio e interface. A arquitetura não depende de Qt ou Tracktion Engine.
