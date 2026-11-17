from fastapi import FastAPI
from pydantic import BaseModel
from influxdb_client import InfluxDBClient, Point
from influxdb_client.client.write_api import SYNCHRONOUS

app = FastAPI()

# Configurações do seu InfluxDB Cloud
INFLUX_URL = "https://us-east-1-1.aws.cloud2.influxdata.com"
INFLUX_TOKEN = "MsG2eW9YCDPJ1xdT9zLv4IC5j5CLzkWZ7mNnXpQpWsRiDSEqadcmcy9Zchl9o-ptcys3F5TOBwCnPAVUuAzTmQ=="
INFLUX_ORG = "fa3074a4f7c6bf91"
INFLUX_BUCKET = "lampada_inteligente"

client = InfluxDBClient(url=INFLUX_URL, token=INFLUX_TOKEN, org=INFLUX_ORG)
write_api = client.write_api(write_options=SYNCHRONOUS)

class DadosSensores(BaseModel):
    temperatura: float
    umidade: float
    luminosidade: int
    estado_led: str
    cor_led: str
    botao: bool

@app.post("/api/telemetria")
def receber_dados(dados: DadosSensores):
    ponto = (
        Point("leitura_sensores")
        .field("temperatura", dados.temperatura)
        .field("umidade", dados.umidade)
        .field("luminosidade", dados.luminosidade)
        .field("estado_led", dados.estado_led)
        .field("cor_led", dados.cor_led)
        .field("botao", dados.botao)
    )
    write_api.write(bucket=INFLUX_BUCKET, org=INFLUX_ORG, record=ponto)
    print(f"Salvo no InfluxDB -> Temp: {dados.temperatura}°C | Luz: {dados.luminosidade} | LED: {dados.estado_led} ({dados.cor_led})")
    return {"status": "sucesso", "mensagem": "Dados gravados no InfluxDB"}

@app.get("/")
def verificar_status():
    return {"status": "API IoT online e conectada ao InfluxDB"}