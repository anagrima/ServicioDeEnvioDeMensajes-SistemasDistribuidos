from flask import Flask, request, jsonify

app = Flask(__name__)

@app.route("/normalize", methods=["POST"])
def normalize():
    data = request.get_json(silent=True) or {}
    text = data.get("text", "")
    normalized = " ".join(text.split())
    return jsonify({"normalized_text": normalized})

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000)