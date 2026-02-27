#include "YUVRenderer.h"

static const char* vertexShaderSource = R"(
    #version 330 core
    layout (location = 0) in vec3 aPos;
    layout (location = 1) in vec2 aTexCoord;

    out vec2 TexCoord;

    void main() {
        gl_Position = vec4(aPos, 1.0);
        TexCoord = aTexCoord;
    }
)";

static const char* fragmentShaderSource = R"(
    #version 330 core
    out vec4 FragColor;

    in vec2 TexCoord;

    uniform sampler2D textureY;
    uniform sampler2D textureU;
    uniform sampler2D textureV;
    uniform float videoAspectRatio;
    uniform float widgetAspectRatio;
    uniform bool hasVideo;

    void main() {
        if (!hasVideo) {
            FragColor = vec4(0.101961, 0.101961, 0.101961, 1.0);
            return;
        }

        vec2 adjustedTexCoord = TexCoord;
        
        if (videoAspectRatio > widgetAspectRatio) {
            float scale = widgetAspectRatio / videoAspectRatio;
            adjustedTexCoord.y = (adjustedTexCoord.y - 0.5) / scale + 0.5;
            
            if (adjustedTexCoord.y < 0.0 || adjustedTexCoord.y > 1.0) {
                FragColor = vec4(0.101961, 0.101961, 0.101961, 1.0);
                return;
            }
        } else {
            float scale = videoAspectRatio / widgetAspectRatio;
            adjustedTexCoord.x = (adjustedTexCoord.x - 0.5) / scale + 0.5;
            
            if (adjustedTexCoord.x < 0.0 || adjustedTexCoord.x > 1.0) {
                FragColor = vec4(0.101961, 0.101961, 0.101961, 1.0);
                return;
            }
        }

        float y = texture(textureY, adjustedTexCoord).r;
        float u = texture(textureU, adjustedTexCoord).r - 0.5;
        float v = texture(textureV, adjustedTexCoord).r - 0.5;
    
        float r = y + 1.403 * v;
        float g = y - 0.344 * u - 0.714 * v;
        float b = y + 1.770 * u;
    
        FragColor = vec4(clamp(r, 0.0, 1.0), clamp(g, 0.0, 1.0), clamp(b, 0.0, 1.0), 1.0);
    }
)";

YUVRenderer::YUVRenderer(QWidget* parent)
    : QOpenGLWidget(parent)
    , shader(nullptr)
    , vbo(QOpenGLBuffer::VertexBuffer)
    , ebo(QOpenGLBuffer::IndexBuffer)
    , textureY(0)
    , textureU(0)
    , textureV(0)
    , videoW(0)
    , videoH(0)
    , widgetW(0)
    , widgetH(0)
    , frameReady(false)
    , initialized(false)
    , textureYLocation(-1)
    , textureULocation(-1)
    , textureVLocation(-1)
    , videoAspectRLocation(-1)
    , widgetAspectRLocation(-1)
    , hasVideoLocation(-1) {
}

YUVRenderer::~YUVRenderer() {
    cleanup();
}

void YUVRenderer::updateYUVFrame(const uint8_t* yData, const uint8_t* uData, const uint8_t* vData,
                                 int width, int height,
                                 int yLineSize, int uLineSize, int vLineSize) {
    if (!yData || !uData || !vData) {
        return;
    }
    if (width < 16 || width > 7680 || height < 16 || height > 7680) {
        return;
    }
    if (yLineSize <= 0 || uLineSize <= 0 || vLineSize <= 0) {
        return;
    }

    QMutexLocker locker(&dataMutex);

    videoW = width;
    videoH = height;

    int chromaH = (height + 1) / 2;

    int ySize = yLineSize * height;
    this->yData.resize(ySize);
    memcpy(this->yData.data(), yData, ySize);

    int uSize = uLineSize * chromaH;
    this->uData.resize(uSize);
    memcpy(this->uData.data(), uData, uSize);

    int vSize = vLineSize * chromaH;
    this->vData.resize(vSize);
    memcpy(this->vData.data(), vData, vSize);

    frameReady = true;
    update();
}

void YUVRenderer::clearFrame() {
    QMutexLocker locker(&dataMutex);

    frameReady = false;
    videoW = 0;
    videoH = 0;

    yData.clear();
    uData.clear();
    vData.clear();

    if (initialized) {
        locker.unlock();
        makeCurrent();

        if (textureY) {
            glBindTexture(GL_TEXTURE_2D, textureY);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        }
        if (textureU) {
            glBindTexture(GL_TEXTURE_2D, textureU);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        }
        if (textureV) {
            glBindTexture(GL_TEXTURE_2D, textureV);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 1, 1, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        }
        glBindTexture(GL_TEXTURE_2D, 0);

        doneCurrent();
    }

    update();
}

void YUVRenderer::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.101961f, 0.101961f, 0.101961f, 1.0f);

    if (!initShader()) {
        return;
    }

    initVertex();
    initTexture();

    initialized = true;
}

void YUVRenderer::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT);

    if (!initialized || !shader) {
        return;
    }

    updateTexture();
    updateAspectRatio();

    if (!shader->bind()) {
        return;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureY);
    shader->setUniformValue(textureYLocation, 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, textureU);
    shader->setUniformValue(textureULocation, 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, textureV);
    shader->setUniformValue(textureVLocation, 2);

    vao.bind();
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    vao.release();

    shader->release();
}

void YUVRenderer::resizeGL(int width, int height) {
    widgetW = width;
    widgetH = height;

    glViewport(0, 0, width, height);
    updateAspectRatio();
}

bool YUVRenderer::initShader() {
    shader = new QOpenGLShaderProgram(this);

    if (!shader->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShaderSource)) {
        return false;
    }
    if (!shader->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShaderSource)) {
        return false;
    }
    if (!shader->link()) {
        return false;
    }

    textureYLocation = shader->uniformLocation("textureY");
    textureULocation = shader->uniformLocation("textureU");
    textureVLocation = shader->uniformLocation("textureV");
    videoAspectRLocation = shader->uniformLocation("videoAspectRatio");
    widgetAspectRLocation = shader->uniformLocation("widgetAspectRatio");
    hasVideoLocation = shader->uniformLocation("hasVideo");

    return textureYLocation     >= 0 && textureULocation      >= 0 && textureVLocation >= 0 &&
           videoAspectRLocation >= 0 && widgetAspectRLocation >= 0 && hasVideoLocation >= 0;
}

void YUVRenderer::initVertex() {
    float vertices[] = {
        -1.0f, -1.0f, 0.0f,  0.0f, 1.0f,
         1.0f, -1.0f, 0.0f,  1.0f, 1.0f,
         1.0f,  1.0f, 0.0f,  1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,  0.0f, 0.0f
    };

    unsigned int indices[] = {
        0, 1, 2,
        2, 3, 0
    };

    vao.create();
    vao.bind();

    vbo.create();
    vbo.bind();
    vbo.allocate(vertices, sizeof(vertices));

    ebo.create();
    ebo.bind();
    ebo.allocate(indices, sizeof(indices));

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    vao.release();
    vbo.release();
    ebo.release();
}

void YUVRenderer::initTexture() {
    glGenTextures(1, &textureY);
    glGenTextures(1, &textureU);
    glGenTextures(1, &textureV);

    glBindTexture(GL_TEXTURE_2D, textureY);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, textureU);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, textureV);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void YUVRenderer::updateTexture() {
    if (!initialized) {
        return;
    }

    QMutexLocker locker(&dataMutex);

    if (!frameReady) {
        return;
    }

    if (videoW <= 0 || videoH <= 0) {
        return;
    }

    int chromaW = (videoW + 1) / 2;
    int chromaH = (videoH + 1) / 2;

    glBindTexture(GL_TEXTURE_2D, textureY);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, videoW, videoH, 0, GL_RED, GL_UNSIGNED_BYTE, yData.constData());

    glBindTexture(GL_TEXTURE_2D, textureU);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, chromaW, chromaH, 0, GL_RED, GL_UNSIGNED_BYTE, uData.constData());

    glBindTexture(GL_TEXTURE_2D, textureV);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, chromaW, chromaH, 0, GL_RED, GL_UNSIGNED_BYTE, vData.constData());

    glBindTexture(GL_TEXTURE_2D, 0);

    frameReady = false;
}

void YUVRenderer::updateAspectRatio() {
    if (!initialized || !shader) {
        return;
    }

    shader->bind();

    QMutexLocker locker(&dataMutex);
    bool hasVideo = (videoW > 0 && videoH > 0);

    if (hasVideo && widgetW > 0 && widgetH > 0) {
        float videoAspect = static_cast<float>(videoW) / videoH;
        float widgetAspect = static_cast<float>(widgetW) / widgetH;

        locker.unlock();

        shader->setUniformValue(hasVideoLocation, hasVideo);
        shader->setUniformValue(videoAspectRLocation, videoAspect);
        shader->setUniformValue(widgetAspectRLocation, widgetAspect);
    }
    else {
        locker.unlock();
        shader->setUniformValue(hasVideoLocation, hasVideo);
    }

    shader->release();
}

void YUVRenderer::cleanup() {
    if (!initialized) {
        return;
    }

    makeCurrent();

    if (textureY) {
        glDeleteTextures(1, &textureY);
        textureY = 0;
    }
    if (textureU) {
        glDeleteTextures(1, &textureU);
        textureU = 0;
    }
    if (textureV) {
        glDeleteTextures(1, &textureV);
        textureV = 0;
    }

    vao.destroy();
    vbo.destroy();
    ebo.destroy();

    delete shader;
    shader = nullptr;

    initialized = false;
    doneCurrent();
}