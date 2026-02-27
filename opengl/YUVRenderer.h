#pragma once

#include <QMutex>
#include <QOpenGLBuffer>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>

class YUVRenderer : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    explicit YUVRenderer(QWidget* parent = nullptr);
    ~YUVRenderer();

    void updateYUVFrame(const uint8_t* yData, const uint8_t* uData, const uint8_t* vData,
                        int width, int height,
                        int yLineSize, int uLineSize, int vLineSize);

    void clearFrame();

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int width, int height) override;

private:
    bool initShader();
    void initVertex();
    void initTexture();
    void updateTexture();
    void updateAspectRatio();
    void cleanup();

private:
    QOpenGLShaderProgram* shader;
    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer vbo;
    QOpenGLBuffer ebo;

    GLuint textureY;
    GLuint textureU;
    GLuint textureV;

    int videoW;
    int videoH;
    int widgetW;
    int widgetH;

    bool frameReady;
    bool initialized;

    int textureYLocation;
    int textureULocation;
    int textureVLocation;
    int videoAspectRLocation;
    int widgetAspectRLocation;
    int hasVideoLocation;

    QMutex dataMutex;
    QByteArray yData;
    QByteArray uData;
    QByteArray vData;
};