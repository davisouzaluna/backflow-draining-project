# Monitoramento Virtual de Alagamentos no Recife - IN1061

**Integrantes:** Luciana e Davi  
**Disciplina:** IN1061  

---

## Descrição do Projeto

O projeto propõe o desenvolvimento de um sistema de monitoramento virtual capaz de identificar pontos de alagamento na cidade do Recife, a partir da integração e análise de dados de chuva e de maré.

### O Problema
A ocorrência de alagamentos no Recife é frequente durante períodos de chuva intensa. Quando o volume acumulado de água ultrapassa a capacidade de escoamento do sistema urbanístico, a água se acumula nas ruas e frequentemente retorna pelos próprios pontos e galerias de drenagem.

### Fenômeno de Alagamento Composto (*Compound Flooding*)
O projeto analisa a influência direta da maré nesse processo, sobretudo nos casos em que chuvas intensas coincidem com períodos de maré alta. Essa condição caracteriza o fenômeno de **alagamento composto** (*compound flooding*), no qual a elevação do nível do mar reduz a capacidade de vazão do sistema de drenagem e intensifica o acúmulo de água nas vias públicas, fenômeno amplamente documentado em outras cidades costeiras de baixa altitude.

### A Solução
A ideia central é observar e correlacionar os dados pluviométricos e de maré para identificar preventivamente as condições ambientais associadas a esses eventos. A solução consiste em um **sensor virtual** responsável por integrar e processar:
* **Dados de Chuva:** Disponibilizados pela Agência Pernambucana de Águas e Clima (APAC).
* **Dados de Maré:** Fornecidos pelo Centro de Hidrografia da Marinha do Brasil (CHM).

---

## 🛠️ PRÉ-REQUISITOS E INSTALAÇÃO

Para compilar e executar o projeto em sistemas derivados do Debian/Ubuntu, é necessário instalar as ferramentas de compilação(`cmake`) e as bibliotecas de desenvolvimento em C (`libcurl` e `cJSON`).Nessa versão é recomendável o acesso à internet.

Abra o terminal e execute:

```bash
sudo apt update
sudo apt install -y build-essential cmake libcurl4-openssl-dev libcjson-dev
```
Por fim crie o diretório build, caso não esteja criado e compile, conforme os comandos abaixo:

```bash
mkdir build && cd build
cmake .. && make
```

---

### Referências
- OBARA, Chloe et al. Drainage failure and associated urban impacts under combined sea-level rise and precipitation scenarios. Scientific Reports, v. 15, n. 1, p. 23436, 2025. 

- GAO, Liang et al. Modelling the compound floods upon combined rainfall and storm surge events in a low-lying coastal city. Journal of Hydrology, v. 627, p. 130476, 2023. 
