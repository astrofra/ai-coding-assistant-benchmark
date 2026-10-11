# Notes

## Optimized parameters

Found on : https://ollama.com/SimonPu/Qwen3-Coder

````
      temperature: 0.7
      top_p: 0.8
      # Extra sampling parameters for the OpenAI-compatible Albert endpoint.
      extra_body:
        top_k: 20
        repetition_penalty: 1.05
```